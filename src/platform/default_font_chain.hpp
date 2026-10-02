#pragma once

#include "font/font_runtime.hpp"

#include <ryn/theme.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ryn::detail {

struct FontFilePreference {
    std::filesystem::path path;
    long face_index{};
    std::string family_name;
    // Face selection metadata for application-supplied files. `weight` follows
    // the CSS/DirectWrite scale (400 regular, 700 bold); `italic` selects a
    // slanted face.
    std::uint32_t weight{400};
    bool italic{};
};

struct DefaultFontChainRequest {
    font::FontRasterConfig raster{};
    std::vector<FontFilePreference> preferred_fonts;
    // Optional application-supplied monospace faces. When empty the platform
    // monospace family is resolved instead.
    std::vector<FontFilePreference> preferred_monospace_fonts;
    std::filesystem::path fallback_latin;
    std::filesystem::path fallback_cjk;
};

struct LoadedDefaultFontFace {
    font::FontIdentity identity{};
    std::filesystem::path source_path;
    long face_index{};
    std::string family_name;
    font::FontRasterPolicy raster_policy{};
    bool custom_font{};
    bool system_font{};
    std::uint32_t weight{400};
    bool italic{};
};

// `family` selects the UI or monospace chain, `weight` follows the
// CSS/DirectWrite scale and `italic` requests a slanted face. A weight or slant
// the platform cannot provide falls back to the regular face of the same chain
// and is recorded in `DefaultFontChainResult::diagnostic_fallbacks`.
using DefaultUiFontResolver =
    std::function<std::vector<font::FontIdentity>(SystemFontFamily, std::uint32_t, bool, std::uint32_t)>;

struct DefaultFontChainResult {
    std::vector<LoadedDefaultFontFace> faces;
    // Monospace-only faces discovered for `SystemFontFamily::ui_monospace`. They
    // are released alongside `faces` but are not part of the UI chain.
    std::vector<LoadedDefaultFontFace> monospace_faces;
    std::string diagnostic;
    // Face-selection notes accumulated by the resolver, for example a strong or
    // italic request that had to fall back to the regular face.
    std::vector<std::string> diagnostic_fallbacks;
    bool uses_custom_fonts{};
    bool uses_system_fonts{};
    bool uses_bundled_fallbacks{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !faces.empty() && diagnostic.empty();
    }

    [[nodiscard]] std::vector<font::FontIdentity> identities() const;
    [[nodiscard]] std::vector<font::FontIdentity> monospace_identities() const;
    [[nodiscard]] std::string telemetry_source() const;
    [[nodiscard]] std::string telemetry_families() const;
    [[nodiscard]] std::string telemetry_rendering() const;
};

[[nodiscard]] DefaultFontChainResult load_default_ui_font_chain(font::FontRuntime& fonts,
                                                                const DefaultFontChainRequest& request);

// Resolves one platform face for a family, weight (CSS/DirectWrite scale) and
// slant. Returns `std::nullopt` when the platform has no such face, which lets
// the resolver fall back to the regular face explicitly instead of silently
// returning something else.
[[nodiscard]] std::optional<LoadedDefaultFontFace> resolve_platform_face(SystemFontFamily family, std::uint32_t weight,
                                                                         bool italic);

[[nodiscard]] DefaultUiFontResolver
make_default_ui_font_resolver(font::FontRuntime& fonts, DefaultFontChainResult& initial_chain, float display_scale);

} // namespace ryn::detail
