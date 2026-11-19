#include "platform/directwrite_grayscale.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite_3.h>
#include <wrl/client.h>

#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ryn::detail {
namespace {

using Microsoft::WRL::ComPtr;

class FontMemory final : public IUnknown {
public:
    explicit FontMemory(std::shared_ptr<const std::vector<std::byte>> bytes) : bytes_(std::move(bytes)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid != __uuidof(IUnknown)) {
            return E_NOINTERFACE;
        }
        *object = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    [[nodiscard]] const auto& bytes() const noexcept {
        return *bytes_;
    }

private:
    std::shared_ptr<const std::vector<std::byte>> bytes_;
    std::atomic<ULONG> references_{1};
};

struct DirectWriteState final {
    DirectWriteState() {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory5),
                                       reinterpret_cast<IUnknown**>(factory.GetAddressOf()))) ||
            FAILED(factory->CreateInMemoryFontFileLoader(loader.GetAddressOf())) ||
            FAILED(factory->RegisterFontFileLoader(loader.Get()))) {
            throw std::runtime_error("DirectWrite grayscale font factory or in-memory loader is unavailable.");
        }
    }

    ~DirectWriteState() {
        static_cast<void>(factory->UnregisterFontFileLoader(loader.Get()));
    }

    ComPtr<IDWriteFactory5> factory;
    ComPtr<IDWriteInMemoryFontFileLoader> loader;
};

font::FontError face_error(const char* diagnostic) {
    return {font::FontErrorStage::face_creation, font::FontErrorKind::invalid_font_data, {}, 0, diagnostic};
}

font::FontError raster_error(const char* diagnostic) {
    return {font::FontErrorStage::rasterization, font::FontErrorKind::rasterization_failed, {}, 0, diagnostic};
}

class DirectWriteFontFace final : public font::PlatformFontFace {
public:
    DirectWriteFontFace(std::shared_ptr<DirectWriteState> state, ComPtr<IDWriteFontFace> face)
        : state_(std::move(state)), face_(std::move(face)) {}

    font::FontError rasterize(std::uint32_t glyph, float physical_em, font::GlyphRasterPhase phase,
                              const font::FontRasterPolicy& policy, font::GlyphBitmap& result) override {
        if (glyph >= face_->GetGlyphCount()) {
            return raster_error("DirectWrite glyph index is outside the admitted font face.");
        }
        const auto index = static_cast<UINT16>(glyph);
        DWRITE_GLYPH_METRICS metrics{};
        if (FAILED(face_->GetDesignGlyphMetrics(&index, 1, &metrics, FALSE))) {
            return raster_error("DirectWrite could not read design glyph metrics.");
        }
        DWRITE_FONT_METRICS face_metrics{};
        face_->GetMetrics(&face_metrics);
        if (face_metrics.designUnitsPerEm == 0) {
            return raster_error("DirectWrite font face has no design em.");
        }
        result.advance_x = static_cast<float>(metrics.advanceWidth) * physical_em / face_metrics.designUnitsPerEm;

        const float advance = 0;
        const DWRITE_GLYPH_OFFSET offset{};
        const DWRITE_GLYPH_RUN run{face_.Get(), physical_em, 1, &index, &advance, &offset, FALSE, 0};
        const DWRITE_MATRIX transform{1, 0, 0, 1, font::glyph_raster_phase_offset(phase), 0};
        ComPtr<IDWriteGlyphRunAnalysis> analysis;
        const auto hinting = policy.hinting && policy.hint_style != font::FontHintStyle::none;
        auto* factory = state_->factory.Get();
        constexpr auto rendering = DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC;
        constexpr auto measuring = DWRITE_MEASURING_MODE_NATURAL;
        const auto status = hinting ? static_cast<IDWriteFactory*>(factory)->CreateGlyphRunAnalysis(
                                          &run, 1, &transform, rendering, measuring, 0, 0, analysis.GetAddressOf())
                                    : static_cast<IDWriteFactory2*>(factory)->CreateGlyphRunAnalysis(
                                          &run, &transform, rendering, measuring, DWRITE_GRID_FIT_MODE_DISABLED,
                                          DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0, analysis.GetAddressOf());
        if (FAILED(status)) {
            return raster_error("DirectWrite could not analyze the grayscale glyph run.");
        }
        RECT bounds{};
        constexpr auto texture = DWRITE_TEXTURE_CLEARTYPE_3x1;
        if (FAILED(analysis->GetAlphaTextureBounds(texture, &bounds))) {
            return raster_error("DirectWrite could not measure glyph coverage bounds.");
        }
        const auto width = static_cast<std::int64_t>(bounds.right) - bounds.left;
        const auto height = static_cast<std::int64_t>(bounds.bottom) - bounds.top;
        if (width <= 0 || height <= 0) {
            return {};
        }
        if (bounds.top == std::numeric_limits<LONG>::min() ||
            static_cast<std::uint64_t>(width) * height > std::numeric_limits<UINT32>::max() / 3U) {
            return raster_error("DirectWrite glyph coverage exceeds the R8 raster extent.");
        }
        const auto pixels = static_cast<std::size_t>(width * height);
        std::vector<std::uint8_t> coverage(pixels * 3);
        if (FAILED(analysis->CreateAlphaTexture(texture, &bounds, coverage.data(),
                                                static_cast<UINT32>(coverage.size())))) {
            return raster_error("DirectWrite could not render glyph coverage.");
        }
        // Keep one coverage channel. Three samples improve horizontal
        // coverage without relying on subpixel order or color fringing.
        for (std::size_t i = 0; i < pixels; ++i) {
            coverage[i] = static_cast<std::uint8_t>((coverage[3 * i] + coverage[3 * i + 1] + coverage[3 * i + 2]) / 3);
        }
        coverage.resize(pixels);
        result.coverage = std::move(coverage);
        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(height);
        result.row_stride = result.width;
        result.bearing_x = bounds.left;
        result.bearing_y = -bounds.top;
        result.coverage_transfer = font::GlyphCoverageTransfer::text_contrast_srgb;
        return {};
    }

private:
    std::shared_ptr<DirectWriteState> state_;
    ComPtr<IDWriteFontFace> face_;
};

class DirectWriteRasterizer final : public font::PlatformFontRasterizer {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "directwrite-3x1";
    }

    std::unique_ptr<font::PlatformFontFace> create_face(std::shared_ptr<const std::vector<std::byte>> bytes,
                                                        long face_index, font::FontError& error) override {
        if (!bytes || bytes->empty() || bytes->size() > std::numeric_limits<UINT32>::max() || face_index < 0) {
            error = face_error("DirectWrite font bytes or collection face index is invalid.");
            return {};
        }
        ComPtr<FontMemory> memory;
        memory.Attach(new FontMemory(std::move(bytes)));
        const auto& data = memory.Get()->bytes();
        ComPtr<IDWriteFontFile> file;
        if (FAILED(state_->loader->CreateInMemoryFontFileReference(state_->factory.Get(), data.data(),
                                                                   static_cast<UINT32>(data.size()), memory.Get(),
                                                                   file.GetAddressOf()))) {
            error = face_error("DirectWrite could not admit the immutable font bytes.");
            return {};
        }
        BOOL supported = FALSE;
        DWRITE_FONT_FILE_TYPE file_type{};
        DWRITE_FONT_FACE_TYPE face_type{};
        UINT32 face_count{};
        if (FAILED(file->Analyze(&supported, &file_type, &face_type, &face_count)) || !supported ||
            static_cast<UINT32>(face_index) >= face_count) {
            error = face_error("DirectWrite rejected the requested font collection face.");
            return {};
        }
        auto* source = file.Get();
        ComPtr<IDWriteFontFace> face;
        if (FAILED(state_->factory->CreateFontFace(face_type, 1, &source, static_cast<UINT32>(face_index),
                                                   DWRITE_FONT_SIMULATIONS_NONE, face.GetAddressOf()))) {
            error = face_error("DirectWrite could not create the requested font face.");
            return {};
        }
        return std::make_unique<DirectWriteFontFace>(state_, std::move(face));
    }

private:
    std::shared_ptr<DirectWriteState> state_ = std::make_shared<DirectWriteState>();
};

} // namespace

std::shared_ptr<font::PlatformFontRasterizer> make_directwrite_grayscale_rasterizer() {
    return std::make_shared<DirectWriteRasterizer>();
}

} // namespace ryn::detail
