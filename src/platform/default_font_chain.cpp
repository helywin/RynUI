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
    // Style the platform actually matched, so a request that silently fell back
    // to the regular face can be detected and reported.
    std::uint32_t weight{400};
    bool italic{};
};

#if defined(_WIN32)
template <typename T> class ComHandle final {
public:
    ComHandle() = default;
    ComHandle(const ComHandle&) = delete;
    ComHandle& operator=(const ComHandle&) = delete;

    ComHandle(ComHandle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}

    ComHandle& operator=(ComHandle&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    ~ComHandle() {
        reset();
    }

    [[nodiscard]] T* get() const noexcept {
        return value_;
    }

    [[nodiscard]] T** put() noexcept {
        reset();
        return &value_;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return value_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return value_ != nullptr;
    }

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
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                            size, nullptr, nullptr) != size) {
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
    if (FAILED(loader->QueryInterface(__uuidof(IDWriteLocalFontFileLoader),
                                      reinterpret_cast<void**>(local_loader.put())))) {
        return {};
    }

    const void* key = nullptr;
    UINT32 key_size = 0;
    if (FAILED(file.GetReferenceKey(&key, &key_size))) {
        return {};
    }
    UINT32 path_length = 0;
    if (FAILED(local_loader->GetFilePathLengthFromKey(key, key_size, &path_length))) {
        return {};
    }
    std::wstring path(static_cast<std::size_t>(path_length) + 1U, L'\0');
    if (FAILED(local_loader->GetFilePathFromKey(key, key_size, path.data(), static_cast<UINT32>(path.size())))) {
        return {};
    }
    path.resize(path_length);
    return std::filesystem::path{path};
}

[[nodiscard]] std::optional<FontDescriptor> resolve_family(IDWriteFontCollection& collection,
                                                           std::wstring_view family_name,
                                                           DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                                                           DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL) {
    UINT32 family_index = 0;
    BOOL exists = FALSE;
    const std::wstring name{family_name};
    if (FAILED(collection.FindFamilyName(name.c_str(), &family_index, &exists)) || !exists) {
        return std::nullopt;
    }

    ComHandle<IDWriteFontFamily> family;
    if (FAILED(collection.GetFontFamily(family_index, family.put()))) {
        return std::nullopt;
    }
    // Enumerate the family instead of calling `GetFirstMatchingFont`: for a
    // variable font such as Segoe UI Variable the latter reports the single
    // variable file for every weight, so a bold request would silently reuse the
    // regular face. Picking the closest weight/style pair from the list keeps the
    // requested style visible in the returned descriptor. `IDWriteFontFamily`
    // derives from `IDWriteFontList`, so iteration happens on the family itself.
    const UINT32 font_count = family->GetFontCount();
    if (font_count == 0) {
        return std::nullopt;
    }
    const auto slant_rank = [&](DWRITE_FONT_STYLE candidate) {
        if (candidate == style) {
            return 0;
        }
        const bool italic_side = style == DWRITE_FONT_STYLE_ITALIC || style == DWRITE_FONT_STYLE_OBLIQUE;
        const bool candidate_italic = candidate == DWRITE_FONT_STYLE_ITALIC || candidate == DWRITE_FONT_STYLE_OBLIQUE;
        return italic_side == candidate_italic ? 1 : 2;
    };
    ComHandle<IDWriteFont> font;
    UINT32 best_index = 0;
    int best_rank = 2;
    int best_distance = std::numeric_limits<int>::max();
    for (UINT32 index = 0; index < font_count; ++index) {
        ComHandle<IDWriteFont> candidate;
        if (FAILED(family->GetFont(index, candidate.put())) ||
            candidate->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) {
            continue;
        }
        const int rank = slant_rank(candidate->GetStyle());
        const int distance = std::abs(static_cast<int>(candidate->GetWeight()) - static_cast<int>(weight));
        if (rank < best_rank || (rank == best_rank && distance < best_distance)) {
            best_rank = rank;
            best_distance = distance;
            best_index = index;
        }
    }
    if (best_distance == std::numeric_limits<int>::max() || FAILED(family->GetFont(best_index, font.put()))) {
        return std::nullopt;
    }
    const auto matched_weight = font->GetWeight();
    const auto matched_style = font->GetStyle();
    ComHandle<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(face.put()))) {
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
        static_cast<std::uint32_t>(matched_weight),
        matched_style != DWRITE_FONT_STYLE_NORMAL,
    };
}

[[nodiscard]] bool same_face(const FontDescriptor& left, const FontDescriptor& right);
void append_unique(std::vector<FontDescriptor>& descriptors, FontDescriptor descriptor);

enum class PlatformFontRole {
    ui,
    monospace,
};

[[nodiscard]] std::vector<FontDescriptor> windows_system_fonts(PlatformFontRole role) {
    constexpr std::array latin_families{
        std::wstring_view{L"Segoe UI Variable Text"},
        std::wstring_view{L"Segoe UI Variable"},
        std::wstring_view{L"Segoe UI"},
    };
    // Cascadia Mono ships with Windows Terminal and Consolas with the OS.
    constexpr std::array monospace_families{
        std::wstring_view{L"Cascadia Mono"},
        std::wstring_view{L"Consolas"},
        std::wstring_view{L"Lucida Console"},
    };
    ComHandle<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(factory.put())))) {
        return {};
    }
    ComHandle<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontCollection(collection.put()))) {
        return {};
    }

    std::vector<FontDescriptor> result;
    if (role == PlatformFontRole::monospace) {
        for (const auto family : monospace_families) {
            if (auto resolved = resolve_family(*collection.get(), family)) {
                append_unique(result, std::move(*resolved));
            }
        }
        return result;
    }
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

// Resolves every named candidate at the requested weight and slant. A family
// that has no such face is skipped so the caller can continue to the next
// candidate instead of silently receiving the regular face.
[[nodiscard]] std::vector<FontDescriptor> windows_styled_fonts(PlatformFontRole role, std::uint32_t weight,
                                                               bool italic) {
    constexpr std::array latin_families{
        std::wstring_view{L"Segoe UI Variable Text"},
        std::wstring_view{L"Segoe UI Variable"},
        std::wstring_view{L"Segoe UI"},
    };
    constexpr std::array monospace_families{
        std::wstring_view{L"Cascadia Mono"},
        std::wstring_view{L"Consolas"},
        std::wstring_view{L"Lucida Console"},
    };
    ComHandle<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(factory.put())))) {
        return {};
    }
    ComHandle<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontCollection(collection.put()))) {
        return {};
    }
    const auto style = italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
    std::vector<FontDescriptor> result;
    const auto resolve_real_style = [&](std::wstring_view name) {
        auto resolved = resolve_family(*collection.get(), name, static_cast<DWRITE_FONT_WEIGHT>(weight), style);
        if (!resolved || resolved->italic != italic) {
            return std::optional<FontDescriptor>{};
        }
        if (weight != 400U || italic) {
            const auto regular = resolve_family(*collection.get(), name);
            // FreeType loads the default variation coordinates from a file.
            // DirectWrite can expose several named instances of that same file,
            // but a path/index descriptor cannot carry their axis coordinates.
            // Continue to a static family instead of claiming that metadata alone
            // changed the rendered weight or slant.
            if (regular && resolved->path == regular->path && resolved->face_index == regular->face_index) {
                return std::optional<FontDescriptor>{};
            }
        }
        return resolved;
    };
    if (role == PlatformFontRole::monospace) {
        for (const auto family : monospace_families) {
            if (auto resolved = resolve_real_style(family)) {
                append_unique(result, std::move(*resolved));
            }
        }
        return result;
    }
    for (const auto family : latin_families) {
        if (auto resolved = resolve_real_style(family)) {
            resolved->coverage_probe = U'A';
            result.push_back(std::move(*resolved));
            break;
        }
    }
    if (auto resolved = resolve_real_style(L"Microsoft YaHei UI")) {
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
    if (FcPatternGetBool(&pattern, FC_EMBEDDED_BITMAP, 0, &boolean) == FcResultMatch) {
        policy.embedded_bitmap = boolean == FcTrue;
    }
    return policy;
}

// Maps the CSS/DirectWrite weight scale to the Fontconfig weight constants.
[[nodiscard]] int fontconfig_weight(std::uint32_t weight) noexcept {
    if (weight <= 150U) {
        return FC_WEIGHT_THIN;
    }
    if (weight <= 250U) {
        return FC_WEIGHT_EXTRALIGHT;
    }
    if (weight <= 350U) {
        return FC_WEIGHT_LIGHT;
    }
    if (weight <= 450U) {
        return FC_WEIGHT_REGULAR;
    }
    if (weight <= 550U) {
        return FC_WEIGHT_MEDIUM;
    }
    if (weight <= 650U) {
        return FC_WEIGHT_DEMIBOLD;
    }
    if (weight <= 750U) {
        return FC_WEIGHT_BOLD;
    }
    if (weight <= 850U) {
        return FC_WEIGHT_EXTRABOLD;
    }
    return FC_WEIGHT_BLACK;
}

// Resolves one Fontconfig generic family. `generic_family` is a Fontconfig alias
// such as `sans-serif` or `monospace`, so the platform decides which concrete
// face backs it. `weight` and `italic` are requested explicitly; a face the
// platform cannot match falls back to the family default, which the caller
// detects by comparing the requested and returned style.
[[nodiscard]] std::optional<FontDescriptor>
resolve_fontconfig_family(FcConfig& config, const char* language, char32_t coverage_probe, const char* generic_family,
                          const char* fallback_name, std::optional<std::uint32_t> weight = std::nullopt,
                          bool italic = false) {
    UniqueFcPattern request{FcPatternCreate()};
    if (!request ||
        FcPatternAddString(request.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(generic_family)) == FcFalse ||
        FcPatternAddString(request.get(), FC_LANG, reinterpret_cast<const FcChar8*>(language)) == FcFalse) {
        return std::nullopt;
    }
    if (weight.has_value() && FcPatternAddInteger(request.get(), FC_WEIGHT, fontconfig_weight(*weight)) == FcFalse) {
        return std::nullopt;
    }
    if (italic && FcPatternAddInteger(request.get(), FC_SLANT, FC_SLANT_ITALIC) == FcFalse) {
        return std::nullopt;
    }
    if (FcConfigSubstitute(&config, request.get(), FcMatchPattern) == FcFalse) {
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
    if (FcPatternGetString(match.get(), FC_FILE, 0, &path) != FcResultMatch ||
        FcPatternGetInteger(match.get(), FC_INDEX, 0, &face_index) != FcResultMatch) {
        return std::nullopt;
    }
    static_cast<void>(FcPatternGetString(match.get(), FC_FAMILY, 0, &family));
    // Report the matched style so callers can tell a real match from a fallback.
    int matched_weight = FC_WEIGHT_REGULAR;
    int matched_slant = FC_SLANT_ROMAN;
    static_cast<void>(FcPatternGetInteger(match.get(), FC_WEIGHT, 0, &matched_weight));
    static_cast<void>(FcPatternGetInteger(match.get(), FC_SLANT, 0, &matched_slant));
    return FontDescriptor{
        std::filesystem::path{reinterpret_cast<const char*>(path)},
        static_cast<long>(face_index),
        family != nullptr ? reinterpret_cast<const char*>(family) : std::string{fallback_name},
        coverage_probe,
        false,
        true,
        fontconfig_raster_policy(*match),
        static_cast<std::uint32_t>(matched_weight),
        matched_slant != FC_SLANT_ROMAN,
    };
}

[[nodiscard]] std::optional<FontDescriptor> resolve_fontconfig_default(FcConfig& config, const char* language,
                                                                       char32_t coverage_probe) {
    return resolve_fontconfig_family(config, language, coverage_probe, "sans-serif", "LinuxSystemSans");
}

// Resolves the platform family for the requested role, weight and slant. On
// Linux the generic family alias lets Fontconfig pick the concrete family and
// its matched style is reported back so callers can detect a fallback.
[[nodiscard]] std::optional<FontDescriptor> platform_styled_descriptor(PlatformFontRole role, std::uint32_t weight,
                                                                       bool italic) {
    UniqueFcConfig config{FcInitLoadConfigAndFonts()};
    if (!config) {
        return std::nullopt;
    }
    const bool monospace = role == PlatformFontRole::monospace;
    return resolve_fontconfig_family(*config, "en", U'\0', monospace ? "monospace" : "sans-serif",
                                     monospace ? "LinuxSystemMonospace" : "LinuxSystemSans", weight, italic);
}

[[nodiscard]] std::vector<FontDescriptor> platform_system_fonts(PlatformFontRole role) {
    UniqueFcConfig config{FcInitLoadConfigAndFonts()};
    if (!config) {
        return {};
    }
    std::vector<FontDescriptor> result;
    if (role == PlatformFontRole::monospace) {
        if (auto mono = resolve_fontconfig_family(*config, "en", U'\0', "monospace", "LinuxSystemMonospace")) {
            result.push_back(std::move(*mono));
        }
        return result;
    }
    if (auto latin = resolve_fontconfig_default(*config, "en", U'A')) {
        result.push_back(std::move(*latin));
    }
    if (auto cjk = resolve_fontconfig_default(*config, "zh-cn", U'中')) {
        result.push_back(std::move(*cjk));
    }
    return result;
}
#else
[[nodiscard]] std::vector<FontDescriptor> platform_system_fonts(PlatformFontRole) {
    return {};
}

[[nodiscard]] std::optional<FontDescriptor> platform_styled_descriptor(PlatformFontRole, std::uint32_t, bool) {
    return std::nullopt;
}
#endif

#if defined(_WIN32)
[[nodiscard]] std::vector<FontDescriptor> platform_system_fonts(PlatformFontRole role) {
    return windows_system_fonts(role);
}

[[nodiscard]] std::optional<FontDescriptor> platform_styled_descriptor(PlatformFontRole role, std::uint32_t weight,
                                                                       bool italic) {
    const auto candidates = windows_styled_fonts(role, weight, italic);
    if (candidates.empty()) {
        return std::nullopt;
    }
    return candidates.front();
}
#endif

[[nodiscard]] bool same_face(const FontDescriptor& left, const FontDescriptor& right) {
    return left.face_index == right.face_index && left.path == right.path;
}

void append_unique(std::vector<FontDescriptor>& descriptors, FontDescriptor descriptor) {
    if (std::ranges::none_of(descriptors, [&](const auto& existing) { return same_face(existing, descriptor); })) {
        descriptors.push_back(std::move(descriptor));
    }
}

// Cache key for one resolved face set. Declared at namespace scope because a
// local class cannot define a friend operator.
struct FaceKey final {
    SystemFontFamily family{SystemFontFamily::ui_sans};
    std::uint32_t weight{400};
    bool italic{};
    std::uint32_t pixel_size{};

    friend bool operator<(const FaceKey& left, const FaceKey& right) noexcept {
        if (left.pixel_size != right.pixel_size) {
            return left.pixel_size < right.pixel_size;
        }
        if (left.family != right.family) {
            return left.family < right.family;
        }
        if (left.weight != right.weight) {
            return left.weight < right.weight;
        }
        return left.italic < right.italic;
    }
};

void release_loaded(font::FontRuntime& fonts, DefaultFontChainResult& result) noexcept {
    for (auto face = result.monospace_faces.rbegin(); face != result.monospace_faces.rend(); ++face) {
        static_cast<void>(fonts.remove_font(face->identity));
    }
    result.monospace_faces.clear();
    for (auto face = result.faces.rbegin(); face != result.faces.rend(); ++face) {
        static_cast<void>(fonts.remove_font(face->identity));
    }
    result.faces.clear();
}

[[nodiscard]] bool covers(font::FontRuntime& fonts, const DefaultFontChainResult& result, char32_t codepoint) {
    const auto identities = result.identities();
    return static_cast<bool>(fonts.find_glyph(identities, codepoint, std::nullopt));
}

[[nodiscard]] std::optional<LoadedDefaultFontFace> load_face(font::FontRuntime& fonts, const FontDescriptor& descriptor,
                                                             font::FontRasterConfig raster) {
    if (descriptor.raster_policy.has_value()) {
        raster.policy = *descriptor.raster_policy;
    }
    const auto loaded = fonts.load_font_file(descriptor.path, descriptor.face_index, raster);
    if (!loaded) {
        return std::nullopt;
    }
    return LoadedDefaultFontFace{
        loaded.font,       descriptor.path,        descriptor.face_index,  descriptor.family_name,
        raster.policy,     descriptor.custom_font, descriptor.system_font, descriptor.weight,
        descriptor.italic,
    };
}

[[nodiscard]] bool load_descriptor(font::FontRuntime& fonts, const FontDescriptor& descriptor,
                                   font::FontRasterConfig raster, DefaultFontChainResult& result) {
    auto face = load_face(fonts, descriptor, raster);
    if (!face.has_value()) {
        return false;
    }
    result.faces.push_back(std::move(*face));
    result.uses_custom_fonts = result.uses_custom_fonts || descriptor.custom_font;
    result.uses_system_fonts = result.uses_system_fonts || descriptor.system_font;
    result.uses_bundled_fallbacks =
        result.uses_bundled_fallbacks || (!descriptor.custom_font && !descriptor.system_font);
    return true;
}

// Monospace faces are separate from the UI chain, so a failure to resolve them
// is not fatal: the UI chain still covers the codepoint.
void load_monospace_descriptor(font::FontRuntime& fonts, const FontDescriptor& descriptor,
                               font::FontRasterConfig raster, DefaultFontChainResult& result) {
    auto face = load_face(fonts, descriptor, raster);
    if (face.has_value()) {
        result.monospace_faces.push_back(std::move(*face));
    }
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

// The monospace chain is the monospace faces followed by the whole UI chain, so
// a codepoint the monospace family does not cover still resolves to a readable
// face instead of a missing glyph.
std::vector<font::FontIdentity> DefaultFontChainResult::monospace_identities() const {
    std::vector<font::FontIdentity> result;
    result.reserve(monospace_faces.size() + faces.size());
    const auto append_unique = [&result](font::FontIdentity identity) {
        if (std::ranges::find(result, identity) == result.end()) {
            result.push_back(identity);
        }
    };
    for (const auto& face : monospace_faces) {
        append_unique(face.identity);
    }
    for (const auto& face : faces) {
        append_unique(face.identity);
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

DefaultFontChainResult load_default_ui_font_chain(font::FontRuntime& fonts, const DefaultFontChainRequest& request) {
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
                                       preferred.weight,
                                       preferred.italic,
                                   });
    }
    for (const auto& descriptor : descriptors) {
        if (!load_descriptor(fonts, descriptor, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Configured custom UI font could not be loaded: " + descriptor.path.string();
            return result;
        }
    }

    for (auto descriptor : platform_system_fonts(PlatformFontRole::ui)) {
        if (descriptor.coverage_probe != U'\0' && covers(fonts, result, descriptor.coverage_probe)) {
            continue;
        }
        if (std::ranges::none_of(descriptors, [&](const auto& existing) { return same_face(existing, descriptor); })) {
            static_cast<void>(load_descriptor(fonts, descriptor, request.raster, result));
            descriptors.push_back(std::move(descriptor));
        }
    }

    if (!covers(fonts, result, U'A')) {
        const FontDescriptor fallback{
            request.fallback_latin, 0, "BundledLatinFallback", U'A', false, false, {},
        };
        if (!load_descriptor(fonts, fallback, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Default UI font chain could not load a Latin face.";
            return result;
        }
    }
    if (!covers(fonts, result, U'中')) {
        const FontDescriptor fallback{
            request.fallback_cjk, 0, "BundledCjkFallback", U'中', false, false, {},
        };
        if (!load_descriptor(fonts, fallback, request.raster, result)) {
            release_loaded(fonts, result);
            result.diagnostic = "Default UI font chain could not load a CJK face.";
            return result;
        }
    }

    // Monospace faces are resolved after the UI chain and are never fatal: a
    // missing monospace family only means `ui_monospace` falls back to the UI
    // chain, which the resolver still reports through the same identity list.
    std::vector<FontDescriptor> monospace;
    for (const auto& preferred : request.preferred_monospace_fonts) {
        append_unique(monospace, {
                                     preferred.path,
                                     preferred.face_index,
                                     preferred.family_name.empty() ? "CustomMonospaceFont" : preferred.family_name,
                                     U'\0',
                                     true,
                                     false,
                                     {},
                                     preferred.weight,
                                     preferred.italic,
                                 });
    }
    for (auto descriptor : platform_system_fonts(PlatformFontRole::monospace)) {
        append_unique(monospace, std::move(descriptor));
    }
    for (const auto& descriptor : monospace) {
        if (std::ranges::any_of(result.faces, [&](const auto& existing) {
                return existing.source_path == descriptor.path && existing.face_index == descriptor.face_index;
            })) {
            continue;
        }
        load_monospace_descriptor(fonts, descriptor, request.raster, result);
    }
    return result;
}

std::optional<LoadedDefaultFontFace> resolve_platform_face(SystemFontFamily family, std::uint32_t weight, bool italic) {
    const auto role = family == SystemFontFamily::ui_monospace ? PlatformFontRole::monospace : PlatformFontRole::ui;
    auto descriptor = platform_styled_descriptor(role, weight, italic);
    if (!descriptor.has_value()) {
        return std::nullopt;
    }
    return LoadedDefaultFontFace{
        {},
        descriptor->path,
        descriptor->face_index,
        descriptor->family_name,
        descriptor->raster_policy.value_or(font::FontRasterPolicy{}),
        descriptor->custom_font,
        descriptor->system_font,
        descriptor->weight,
        descriptor->italic,
    };
}

DefaultUiFontResolver make_default_ui_font_resolver(font::FontRuntime& fonts, DefaultFontChainResult& initial_chain,
                                                    float display_scale) {
    if (!initial_chain || !std::isfinite(display_scale) || display_scale <= 0.0F) {
        throw std::invalid_argument("Default UI font resolver requires a loaded chain and positive display scale");
    }

    struct ResolverState final {
        font::FontRuntime* fonts{};
        DefaultFontChainResult* chain{};
        float display_scale{1.0F};
        std::map<FaceKey, std::vector<font::FontIdentity>> cache;
    };

    auto state = std::make_shared<ResolverState>();
    state->fonts = &fonts;
    state->chain = &initial_chain;
    state->display_scale = display_scale;

    // The startup chain is the regular face set, so seed the cache for the
    // regular requests at the startup pixel size. Styled requests resolve lazily
    // because a weight/slant matrix would otherwise load many unused faces.
    for (const auto family : {SystemFontFamily::ui_sans, SystemFontFamily::ui_monospace}) {
        const bool monospace = family == SystemFontFamily::ui_monospace;
        const auto& source = monospace ? initial_chain.monospace_faces : initial_chain.faces;
        if (source.empty() || (!monospace && source.empty())) {
            continue;
        }
        const auto metrics = fonts.metrics(source.front().identity);
        if (!metrics || std::abs(metrics.metrics.display_scale - display_scale) >= 0.0001F) {
            continue;
        }
        state->cache.emplace(FaceKey{family, 400, false, metrics.metrics.logical_pixel_size},
                             monospace ? initial_chain.monospace_identities() : initial_chain.identities());
    }

    return [state](SystemFontFamily family, std::uint32_t weight, bool italic, std::uint32_t pixel_size) {
        const FaceKey key{family, weight, italic, pixel_size};
        if (const auto found = state->cache.find(key); found != state->cache.end()) {
            return found->second;
        }
        const bool monospace = family == SystemFontFamily::ui_monospace;
        const auto raster = [&](const LoadedDefaultFontFace& face) {
            return font::FontRasterConfig{pixel_size, state->display_scale, face.raster_policy};
        };
        const auto reload =
            [&](std::span<const LoadedDefaultFontFace> source) -> std::optional<std::vector<font::FontIdentity>> {
            std::vector<font::FontIdentity> identities;
            identities.reserve(source.size());
            for (const auto& face : source) {
                const auto loaded = state->fonts->load_font_file(face.source_path, face.face_index, raster(face));
                if (!loaded) {
                    return std::nullopt;
                }
                identities.push_back(loaded.font);
            }
            return identities;
        };

        // A styled request resolves real faces for the requested weight and
        // slant. Two chains take part: `own` is the requested family and
        // `fallback` is the UI chain that a monospace request appends so an
        // uncovered codepoint still resolves.
        const std::vector<LoadedDefaultFontFace>& own = monospace ? state->chain->monospace_faces : state->chain->faces;
        const std::vector<LoadedDefaultFontFace>& fallback = state->chain->faces;
        std::vector<LoadedDefaultFontFace> styled;
        const auto already_loaded = [&](const LoadedDefaultFontFace& face) {
            const auto matches = [&](const LoadedDefaultFontFace& existing) {
                return existing.source_path == face.source_path && existing.face_index == face.face_index;
            };
            return std::ranges::any_of(own, matches) || std::ranges::any_of(fallback, matches);
        };
        const bool wants_style = weight != 400U || italic;
        if (wants_style) {
            if (auto resolved = resolve_platform_face(family, weight, italic)) {
                // Accept any face the platform matched that differs from the
                // regular chain. Requiring an exact weight value would reject
                // valid faces, because a family's bold face is not guaranteed to
                // report the requested scale value (a variable font such as Segoe
                // UI Variable reports one file for every weight).
                if (already_loaded(*resolved)) {
                    state->chain->diagnostic_fallbacks.push_back(
                        "requested weight " + std::to_string(weight) + (italic ? " italic" : "") +
                        " resolved to an already loaded face; no styled face available");
                } else {
                    styled.push_back(std::move(*resolved));
                }
            } else {
                state->chain->diagnostic_fallbacks.push_back("requested weight " + std::to_string(weight) +
                                                             (italic ? " italic" : "") +
                                                             " could not be resolved; using the regular face");
            }
        }

        std::vector<font::FontIdentity> identities;
        for (const auto& face : styled) {
            const auto loaded = state->fonts->load_font_file(face.source_path, face.face_index, raster(face));
            if (loaded && std::ranges::find(identities, loaded.font) == identities.end()) {
                identities.push_back(loaded.font);
            }
        }
        // The regular faces of the requested family always follow the styled
        // face, so a styled face only FRONTs the chain instead of replacing the
        // regular face of the same family.
        const auto append = [&](std::span<const LoadedDefaultFontFace> source) {
            const auto loaded = reload(source);
            if (!loaded.has_value()) {
                return false;
            }
            for (const auto identity : *loaded) {
                if (std::ranges::find(identities, identity) == identities.end()) {
                    identities.push_back(identity);
                }
            }
            return true;
        };
        if (!append(own)) {
            return std::vector<font::FontIdentity>{};
        }
        if (monospace && !append(fallback)) {
            return std::vector<font::FontIdentity>{};
        }
        if (identities.empty()) {
            return std::vector<font::FontIdentity>{};
        }
        state->cache.emplace(key, identities);
        return identities;
    };
}

} // namespace ryn::detail
