#include "platform/fontations_grayscale.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace ryn::detail {
namespace {

extern "C" bool rynui_fontations_draw_outline(const std::uint8_t* data, std::size_t length, std::uint32_t face,
                                              std::uint32_t glyph, float em, std::uint8_t hint_style, void* context,
                                              void (*emit)(void*, std::uint8_t, const float*), float* advance);

void append_path(void* context, std::uint8_t verb, const float* p) {
    auto& path = *static_cast<SkPathBuilder*>(context);
    switch (verb) {
    case 'M':
        path.moveTo(p[0], p[1]);
        break;
    case 'L':
        path.lineTo(p[0], p[1]);
        break;
    case 'Q':
        path.quadTo(p[0], p[1], p[2], p[3]);
        break;
    case 'C':
        path.cubicTo(p[0], p[1], p[2], p[3], p[4], p[5]);
        break;
    case 'Z':
        path.close();
        break;
    }
}

font::FontError raster_error(std::string message) {
    return {.stage = font::FontErrorStage::rasterization,
            .kind = font::FontErrorKind::rasterization_failed,
            .diagnostic = std::move(message)};
}

class FontationsFace final : public font::PlatformFontFace {
public:
    FontationsFace(std::shared_ptr<const std::vector<std::byte>> bytes, std::uint32_t index)
        : bytes_(std::move(bytes)), index_(index) {}

    font::FontError rasterize(std::uint32_t glyph, float physical_em, font::GlyphRasterPhase phase,
                              const font::FontRasterPolicy& policy, font::GlyphBitmap& result) override {
        SkPathBuilder outline;
        const auto hint_style = policy.hinting ? policy.hint_style : font::FontHintStyle::none;
        if (!rynui_fontations_draw_outline(reinterpret_cast<const std::uint8_t*>(bytes_->data()), bytes_->size(),
                                           index_, glyph, physical_em, static_cast<std::uint8_t>(hint_style), &outline,
                                           &append_path, &result.advance_x)) {
            return raster_error("Fontations could not generate the glyph outline.");
        }
        outline.offset(font::glyph_raster_phase_offset(phase), 0);
        const auto path = outline.detach();
        if (path.isEmpty()) {
            return {};
        }
        const auto extent = path.getBounds();
        if (!extent.isFinite() || extent.left() <= -16777216.0F || extent.top() <= -16777216.0F ||
            extent.right() >= 16777216.0F || extent.bottom() >= 16777216.0F) {
            return raster_error("Fontations glyph outline exceeds the physical raster extent.");
        }
        const auto bounds = extent.roundOut();
        const auto width = bounds.width();
        const auto height = bounds.height();
        if (width <= 0 || height <= 0) {
            return {};
        }
        if (static_cast<std::uint64_t>(width) * height > std::numeric_limits<std::uint32_t>::max()) {
            return raster_error("Fontations glyph coverage exceeds the R8 raster extent.");
        }

        SkBitmap mask;
        if (!mask.tryAllocPixels(SkImageInfo::MakeA8(width, height))) {
            return raster_error("Skia could not allocate the glyph coverage mask.");
        }
        SkCanvas canvas(mask);
        canvas.clear(SK_ColorTRANSPARENT);
        canvas.translate(-static_cast<float>(bounds.left()), -static_cast<float>(bounds.top()));
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SK_ColorWHITE);
        canvas.drawPath(path, paint);

        result.coverage.resize(static_cast<std::size_t>(width) * height);
        for (int row = 0; row < height; ++row) {
            std::memcpy(result.coverage.data() + static_cast<std::size_t>(row) * width, mask.getAddr8(0, row), width);
        }
        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(height);
        result.row_stride = result.width;
        result.bearing_x = bounds.left();
        result.bearing_y = -bounds.top();
        result.coverage_transfer = font::GlyphCoverageTransfer::text_contrast_gamma12;
        return {};
    }

private:
    std::shared_ptr<const std::vector<std::byte>> bytes_;
    std::uint32_t index_;
};

class FontationsRasterizer final : public font::PlatformFontRasterizer {
public:
    std::string_view name() const noexcept override {
        return "fontations-skia-a8";
    }

    bool snaps_horizontal() const noexcept override {
        return true;
    }

    bool snaps_shaping_positions() const noexcept override {
        return true;
    }

    std::unique_ptr<font::PlatformFontFace> create_face(std::shared_ptr<const std::vector<std::byte>> bytes, long index,
                                                        font::FontError& error) override {
        if (!bytes || bytes->empty() || index < 0 || static_cast<unsigned long>(index) > UINT32_MAX) {
            error = {.stage = font::FontErrorStage::face_creation,
                     .kind = font::FontErrorKind::invalid_font_data,
                     .diagnostic = "Invalid Fontations byte resource or face index."};
            return {};
        }
        error = {};
        return std::make_unique<FontationsFace>(std::move(bytes), static_cast<std::uint32_t>(index));
    }
};

} // namespace

std::shared_ptr<font::PlatformFontRasterizer> make_fontations_grayscale_rasterizer() {
    return std::make_shared<FontationsRasterizer>();
}

} // namespace ryn::detail
