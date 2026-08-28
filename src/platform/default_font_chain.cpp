#include "platform/default_font_chain.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <memory>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite.h>
#elif defined(__linux__)
#include <fontconfig/fontconfig.h>
#endif

namespace ryn::detail {
namespace {

struct FontDescriptor {
    std::filesystem::path path;
    long face_index{};
    std::string family_name;
    char32_t coverage_probe{};
    bool custom_font{};
    bool system_font{};
    std::optional<font::FontRasterPolicy> raster_policy;
};

#if defined(_WIN32)
template <typename T>
class ComHandle final {
public:
    ComHandle() = default;
    ComHandle(const ComHandle&) = delete;
    ComHandle& operator=(const ComHandle&) = delete;

    ComHandle(ComHandle&& other) noexcept
        : value_(std::exchange(other.value_, nullptr)) {}

    ComHandle& operator=(ComHandle&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    ~ComHandle() { reset(); }

    [[nodiscard]] T* get() const noexcept { return value_; }
    [[nodiscard]] T** put() noexcept {
        reset();
        return &value_;
    }
    [[nodiscard]] T* operator->() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }

private:
    void reset() noexcept {
        if (value_ != nullptr) {
            value_->Release();
            value_ = nullptr;
        }
    }

    T* value_{};
};

[[nodiscard]] std::string utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size,
            nullptr,
            nullptr) != size) {
        return {};
    }
    return result;
}

[[nodiscard]] std::filesystem::path local_font_path(IDWriteFontFile& file) {
    ComHandle<IDWriteFontFileLoader> loader;
    if (FAILED(file.GetLoader(loader.put()))) {
        return {};
    }
    ComHandle<IDWriteLocalFontFileLoader> local_loader;
    if (FAILED(loader->QueryInterface(
            __uuidof(IDWriteLocalFontFileLoader),
            reinterpret_cast<void**>(local_loader.put())))) {
        return {};
    }

    const void* key = nullptr;
    UINT32 key_size = 0;
    if (FAILED(file.GetReferenceKey(&key, &key_size))) {
        return {};
    }
    UINT32 path_length = 0;
    if (FAILED(local_loader->GetFilePathLengthFromKey(
            key, key_size, &path_length))) {
        return {};
    }
    std::wstring path(static_cast<std::size_t>(path_length) + 1U, L'\0');
    if (FAILED(local_loader->GetFilePathFromKey(
            key,
            key_size,
            path.data(),
            static_cast<UINT32>(path.size())))) {
        return {};
    }
    path.resize(path_length);
    return std::filesystem::path{path};
}

[[nodiscard]] std::optional<FontDescriptor> resolve_family(
    IDWriteFontCollection& collection,
    std::wstring_view family_name) {
    UINT32 family_index = 0;
    BOOL exists = FALSE;
    const std::wstring name{family_name};
    if (FAILED(collection.FindFamilyName(name.c_str(), &family_index, &exists))
            || !exists) {
        return std::nullopt;
    }

    ComHandle<IDWriteFontFamily> family;
    ComHandle<IDWriteFont> font;
    ComHandle<IDWriteFontFace> face;
    if (FAILED(collection.GetFontFamily(family_index, family.put()))
            || FAILED(family->GetFirstMatchingFont(
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                font.put()))
            || FAILED(font->CreateFontFace(face.put()))) {
        return std::nullopt;
    }

    UINT32 file_count = 0;
    if (FAILED(face->GetFiles(&file_count, nullptr)) || file_count == 0) {
        return std::nullopt;
    }
    std::vector<IDWriteFontFile*> raw_files(file_count);
    if (FAILED(face->GetFiles(&file_count, raw_files.data()))) {
        for (auto* raw : raw_files) {
            if (raw != nullptr) {
                raw->Release();
            }
        }
        return std::nullopt;
    }

    std::filesystem::path path;
    for (auto* raw : raw_files) {
        if (path.empty() && raw != nullptr) {
            path = local_font_path(*raw);
        }
        if (raw != nullptr) {
            raw->Release();
        }
    }
    if (path.empty()) {
        return std::nullopt;
    }
    return FontDescriptor{
        std::move(path),
        static_cast<long>(face->GetIndex()),
        utf8(family_name),
        U'\0',
        false,
        true,
        {},
    };
}

[[nodiscard]] std::vector<FontDescriptor> windows_system_ui_fonts() {
    ComHandle<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.put())))) {
        return {};
    }
    ComHandle<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontCollection(collection.put()))) {
        return {};
    }

    std::vector<FontDescriptor> result;
    constexpr std::array latin_families{
        std::wstring_view{L"Segoe UI Variable Text"},
        std::wstring_view{L"Segoe UI Variable"},
        std::wstring_view{L"Segoe UI"},
    };
    for (const auto family : latin_families) {
        if (auto resolved = resolve_family(*collection.get(), family)) {
            resolved->coverage_probe = U'A';
            result.push_back(std::move(*resolved));
            break;
        }
    }
    if (auto resolved = resolve_family(*collection.get(), L"Microsoft YaHei UI")) {
        resolved->coverage_probe = U'中';
        result.push_back(std::move(*resolved));
    }
    return result;
}
#elif defined(__linux__)
struct FcConfigDeleter {
    void operator()(FcConfig* value) const noexcept {
        if (value != nullptr) {
            FcConfigDestroy(value);
        }
    }
};

struct FcPatternDeleter {
    void operator()(FcPattern* value) const noexcept {
        if (value != nullptr) {
            FcPatternDestroy(value);
        }
    }
};

using UniqueFcConfig = std::unique_ptr<FcConfig, FcConfigDeleter>;
using UniqueFcPattern = std::unique_ptr<FcPattern, FcPatternDeleter>;

[[nodiscard]] font::FontHintStyle font_hint_style(int value) noexcept {
    switch (value) {
    case FC_HINT_NONE:
        return font::FontHintStyle::none;
    case FC_HINT_SLIGHT:
        return font::FontHintStyle::slight;
    case FC_HINT_MEDIUM:
        return font::FontHintStyle::medium;
    case FC_HINT_FULL:
        return font::FontHintStyle::full;
    default:
        return font::FontHintStyle::default_hint;
    }
}

[[nodiscard]] font::FontSubpixelOrder font_subpixel_order(int value) noexcept {
    switch (value) {
    case FC_RGBA_NONE:
        return font::FontSubpixelOrder::none;
    case FC_RGBA_RGB:
        return font::FontSubpixelOrder::rgb;
    case FC_RGBA_BGR:
        return font::FontSubpixelOrder::bgr;
    case FC_RGBA_VRGB:
        return font::FontSubpixelOrder::vertical_rgb;
    case FC_RGBA_VBGR:
        return font::FontSubpixelOrder::vertical_bgr;
    default:
        return font::FontSubpixelOrder::unknown;
    }
}

[[nodiscard]] font::FontLcdFilter font_lcd_filter(int value) noexcept {
    switch (value) {
    case FC_LCD_NONE:
        return font::FontLcdFilter::none;
    case FC_LCD_DEFAULT:
        return font::FontLcdFilter::default_filter;
    case FC_LCD_LIGHT:
        return font::FontLcdFilter::light;
    case FC_LCD_LEGACY:
        return font::FontLcdFilter::legacy;
    default:
        return font::FontLcdFilter::unknown;
    }
}

[[nodiscard]] font::FontRasterPolicy fontconfig_raster_policy(FcPattern& pattern) {
    font::FontRasterPolicy policy;
    FcBool boolean = FcTrue;
    int integer = 0;
    if (FcPatternGetBool(&pattern, FC_ANTIALIAS, 0, &boolean) == FcResultMatch) {
        policy.antialias = boolean == FcTrue;
    }
    if (FcPatternGetBool(&pattern, FC_HINTING, 0, &boolean) == FcResultMatch) {
        policy.hinting = boolean == FcTrue;
    }
    if (FcPatternGetInteger(&pattern, FC_HINT_STYLE, 0, &integer) == FcResultMatch) {
        policy.hint_style = font_hint_style(integer);
    }
    if (FcPatternGetInteger(&pattern, FC_RGBA, 0, &integer) == FcResultMatch) {
        policy.subpixel_order = font_subpixel_order(integer);
    }
    if (FcPatternGetInteger(&pattern, FC_LCD_FILTER, 0, &integer) == FcResultMatch) {
        policy.lcd_filter = font_lcd_filter(integer);
    }
    if (FcPatternGetBool(&pattern, FC_EMBEDDED_BITMAP, 0, &boolean)
            == FcResultMatch) {
        policy.embedded_bitmap = boolean == FcTrue;
    }
    return policy;
}

[[nodiscard]] std::optional<FontDescriptor> resolve_fontconfig_default(
    FcConfig& config,
    const char* language,
    char32_t coverage_probe) {
    UniqueFcPattern request{FcPatternCreate()};
    if (!request
            || FcPatternAddString(
                request.get(), FC_FAMILY,
                reinterpret_cast<const FcChar8*>("sans-serif")) == FcFalse
            || FcPatternAddString(
                request.get(), FC_LANG,
                reinterpret_cast<const FcChar8*>(language)) == FcFalse
            || FcConfigSubstitute(&config, request.get(), FcMatchPattern) == FcFalse) {
        return std::nullopt;
    }
    FcDefaultSubstitute(request.get());

    FcResult match_result = FcResultNoMatch;
    UniqueFcPattern match{FcFontMatch(&config, request.get(), &match_result)};
    if (!match || match_result == FcResultNoMatch) {
        return std::nullopt;
    }

    FcChar8* path = nullptr;
    FcChar8* family = nullptr;
    int face_index = 0;
    if (FcPatternGetString(match.get(), FC_FILE, 0, &path) != FcResultMatch
            || FcPatternGetInteger(match.get(), FC_INDEX, 0, &face_index)
                != FcResultMatch) {
        return std::nullopt;
    }
    static_cast<void>(FcPatternGetString(match.get(), FC_FAMILY, 0, &family));
    return FontDescriptor{
        std::filesystem::path{reinterpret_cast<const char*>(path)},
        static_cast<long>(face_index),
        family != nullptr
            ? reinterpret_cast<const char*>(family)
            : std::string{"LinuxSystemSans"},
        coverage_probe,
        false,
        true,
        fontconfig_raster_policy(*match),
    };
}

[[nodiscard]] std::vector<FontDescriptor> platform_system_ui_fonts() {
    UniqueFcConfig config{FcInitLoadConfigAndFonts()};
    if (!config) {
        return {};
    }
    std::vector<FontDescriptor> result;
    if (auto latin = resolve_fontconfig_default(*config, "en", U'A')) {
        result.push_back(std::move(*latin));
    }
    if (auto cjk = resolve_fontconfig_default(*config, "zh-cn", U'中')) {
        result.push_back(std::move(*cjk));
    }
    return result;
}
#else
[[nodiscard]] std::vector<FontDescriptor> platform_system_ui_fonts() {
    return {};
}
#endif

#if defined(_WIN32)
[[nodiscard]] std::vector<FontDescriptor> platform_system_ui_fonts() {
    return windows_system_ui_fonts();
}
#endif

[[nodiscard]] bool same_face(const FontDescriptor& left, const FontDescriptor& right) {
    return left.face_index == right.face_index && left.path == right.path;
}

void append_unique(
    std::vector<FontDescriptor>& descriptors,
    FontDescriptor descriptor) {
    if (std::ranges::none_of(descriptors, [&](const auto& existing) {
            return same_face(existing, descriptor);
        })) {
        descriptors.push_back(std::move(descriptor));
    }
}

void release_loaded(font::FontRuntime& fonts, DefaultFontChainResult& result) noexcept {
    for (auto face = result.faces.rbegin(); face != result.faces.rend(); ++face) {
        static_cast<void>(fonts.remove_font(face->identity));
    }
    result.faces.clear();
}

[[nodiscard]] bool covers(
    font::FontRuntime& fonts,
    const DefaultFontChainResult& result,
    char32_t codepoint) {
    const auto identities = result.identities();
    return static_cast<bool>(fonts.find_glyph(identities, codepoint, std::nullopt));
}

[[nodiscard]] bool load_descriptor(
    font::FontRuntime& fonts,
    const FontDescriptor& descriptor,
    font::FontRasterConfig raster,
    DefaultFontChainResult& result) {
    if (descriptor.raster_policy.has_value()) {
        raster.policy = *descriptor.raster_policy;
    }
    const auto loaded = fonts.load_font_file(
        descriptor.path,
        descriptor.face_index,
        raster);
    if (!loaded) {
        return false;
    }
    result.faces.push_back({
        loaded.font,
        descriptor.path,
        descriptor.face_index,
        descriptor.family_name,
        raster.policy,
        descriptor.custom_font,
        descriptor.system_font,
    });
    result.uses_custom_fonts = result.uses_custom_fonts || descriptor.custom_font;
    result.uses_system_fonts = result.uses_system_fonts || descriptor.system_font;
    result.uses_bundled_fallbacks = result.uses_bundled_fallbacks
        || (!descriptor.custom_font && !descriptor.system_font);
    return true;
}

[[nodiscard]] std::string_view hint_style_name(font::FontHintStyle value) noexcept {
    switch (value) {
    case font::FontHintStyle::default_hint:
        return "default";
    case font::FontHintStyle::none:
        return "none";
    case font::FontHintStyle::slight:
        return "slight";
    case font::FontHintStyle::medium:
        return "medium";
    case font::FontHintStyle::full:
        return "full";
    }
    return "unknown";
}

[[nodiscard]] std::string_view subpixel_name(font::FontSubpixelOrder value) noexcept {
    switch (value) {
    case font::FontSubpixelOrder::unknown:
        return "unknown";
    case font::FontSubpixelOrder::none:
        return "none";
    case font::FontSubpixelOrder::rgb:
        return "rgb";
    case font::FontSubpixelOrder::bgr:
        return "bgr";
    case font::FontSubpixelOrder::vertical_rgb:
        return "vrgb";
    case font::FontSubpixelOrder::vertical_bgr:
        return "vbgr";
    }
    return "unknown";
}

[[nodiscard]] std::string_view lcd_filter_name(font::FontLcdFilter value) noexcept {
    switch (value) {
    case font::FontLcdFilter::unknown:
        return "unknown";
    case font::FontLcdFilter::none:
        return "none";
    case font::FontLcdFilter::default_filter:
        return "default";
    case font::FontLcdFilter::light:
        return "light";
    case font::FontLcdFilter::legacy:
        return "legacy";
    }
    return "unknown";
}

} // namespace

std::vector<font::FontIdentity> DefaultFontChainResult::identities() const {
    std::vector<font::FontIdentity> result;
    result.reserve(faces.size());
    for (const auto& face : faces) {
        result.push_back(face.identity);
    }
    return result;
}

std::string DefaultFontChainResult::telemetry_source() const {
    std::string result;
    const auto append = [&](std::string_view source) {
        if (!result.empty()) {
            result.push_back('+');
        }
        result.append(source);
    };
    if (uses_custom_fonts) {
        append("custom");
    }
    if (uses_system_fonts) {
        append("system");
    }
    if (uses_bundled_fallbacks) {
        append("bundled");
    }
    return result;
}

std::string DefaultFontChainResult::telemetry_families() const {
    std::string result;
    for (const auto& face : faces) {
        if (!result.empty()) {
            result.push_back(',');
        }
        for (const unsigned char value : face.family_name) {
            result.push_back(std::isspace(value) != 0 ? '_' : static_cast<char>(value));
        }
    }
    return result;
}

std::string DefaultFontChainResult::telemetry_rendering() const {
    std::string result;
    for (const auto& face : faces) {
        if (!result.empty()) {
            result.push_back(';');
        }
        const auto& policy = face.raster_policy;
        result.append(policy.antialias ? "aa=gray" : "aa=mono");
        result.append(",hint=");
        result.append(policy.hinting ? hint_style_name(policy.hint_style) : "none");
        result.append(",rgba=");
        result.append(subpixel_name(policy.subpixel_order));
        result.append(",lcd=");
        result.append(lcd_filter_name(policy.lcd_filter));
        result.append(policy.embedded_bitmap ? ",bitmap=on" : ",bitmap=off");
    }
    return result;
}

DefaultFontChainResult load_default_ui_font_chain(
    font::FontRuntime& fonts,
    const DefaultFontChainRequest& request) {
    DefaultFontChainResult result;
    std::vector<FontDescriptor> descriptors;
    for (const auto& preferred : request.preferred_fonts) {
        append_unique(descriptors, {
            preferred.path,
            preferred.face_index,
            preferred.family_name.empty() ? "CustomFont" : preferred.family_name,
            U'\0',
            true,
            false,
            {},
        });
    }
    for (const auto& descriptor : descriptors) {
        if (!load_descriptor(fonts, descriptor, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Configured custom UI font could not be loaded: "
                + descriptor.path.string();
            return result;
        }
    }

    for (auto descriptor : platform_system_ui_fonts()) {
        if (descriptor.coverage_probe != U'\0'
                && covers(fonts, result, descriptor.coverage_probe)) {
            continue;
        }
        if (std::ranges::none_of(descriptors, [&](const auto& existing) {
                return same_face(existing, descriptor);
            })) {
            static_cast<void>(load_descriptor(fonts, descriptor, request.raster, result));
            descriptors.push_back(std::move(descriptor));
        }
    }

    if (!covers(fonts, result, U'A')) {
        const FontDescriptor fallback{
            request.fallback_latin,
            0,
            "BundledLatinFallback",
            U'A',
            false,
            false,
            {},
        };
        if (!load_descriptor(fonts, fallback, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Default UI font chain could not load a Latin face.";
            return result;
        }
    }
    if (!covers(fonts, result, U'中')) {
        const FontDescriptor fallback{
            request.fallback_cjk,
            0,
            "BundledCjkFallback",
            U'中',
            false,
            false,
            {},
        };
        if (!load_descriptor(fonts, fallback, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Default UI font chain could not load a CJK face.";
            return result;
        }
    }
    return result;
}

DefaultUiFontResolver make_default_ui_font_resolver(
    font::FontRuntime& fonts,
    const DefaultFontChainResult& initial_chain,
    float display_scale) {
    if (!initial_chain || !std::isfinite(display_scale) || display_scale <= 0.0F) {
        throw std::invalid_argument(
            "Default UI font resolver requires a loaded chain and positive display scale");
    }
    struct ResolverState final {
        font::FontRuntime* fonts{};
        std::vector<LoadedDefaultFontFace> faces;
        float display_scale{1.0F};
        std::map<std::uint32_t, std::vector<font::FontIdentity>> cache;
    };
    auto state = std::make_shared<ResolverState>();
    state->fonts = &fonts;
    state->faces = initial_chain.faces;
    state->display_scale = display_scale;
    const auto initial_metrics = fonts.metrics(initial_chain.faces.front().identity);
    if (initial_metrics
            && std::abs(initial_metrics.metrics.display_scale - display_scale)
                < 0.0001F) {
        state->cache.emplace(
            initial_metrics.metrics.logical_pixel_size,
            initial_chain.identities());
    }
    return [state](SystemFontFamily, std::uint32_t, std::uint32_t pixel_size) {
        if (const auto found = state->cache.find(pixel_size);
                found != state->cache.end()) {
            return found->second;
        }
        std::vector<font::FontIdentity> identities;
        identities.reserve(state->faces.size());
        for (const auto& face : state->faces) {
            const auto loaded = state->fonts->load_font_file(
                face.source_path,
                face.face_index,
                font::FontRasterConfig{
                    pixel_size,
                    state->display_scale,
                    face.raster_policy});
            if (!loaded) {
                return std::vector<font::FontIdentity>{};
            }
            identities.push_back(loaded.font);
        }
        state->cache.emplace(pixel_size, identities);
        return identities;
    };
}

} // namespace ryn::detail
