#pragma once

#include "font/font_runtime.hpp"

#include <ryn/theme.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ryn::detail {

struct FontFilePreference {
    std::filesystem::path path;
    long face_index{};
    std::string family_name;
};

struct DefaultFontChainRequest {
    font::FontRasterConfig raster{};
    std::vector<FontFilePreference> preferred_fonts;
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
};

using DefaultUiFontResolver = std::function<std::vector<font::FontIdentity>(
    SystemFontFamily,
    std::uint32_t,
    std::uint32_t)>;

struct DefaultFontChainResult {
    std::vector<LoadedDefaultFontFace> faces;
    std::string diagnostic;
    bool uses_custom_fonts{};
    bool uses_system_fonts{};
    bool uses_bundled_fallbacks{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !faces.empty() && diagnostic.empty();
    }

    [[nodiscard]] std::vector<font::FontIdentity> identities() const;
    [[nodiscard]] std::string telemetry_source() const;
    [[nodiscard]] std::string telemetry_families() const;
    [[nodiscard]] std::string telemetry_rendering() const;
};

[[nodiscard]] DefaultFontChainResult load_default_ui_font_chain(
    font::FontRuntime& fonts,
    const DefaultFontChainRequest& request);

[[nodiscard]] DefaultUiFontResolver make_default_ui_font_resolver(
    font::FontRuntime& fonts,
    const DefaultFontChainResult& initial_chain,
    float display_scale);

} // namespace ryn::detail
