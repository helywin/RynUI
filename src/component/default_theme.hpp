#pragma once

#include <array>
#include <cstdint>

namespace ryn::detail {

enum class DefaultFontFamily : std::uint8_t {
    system_ui_sans,
};

struct DefaultTextTypographyToken final {
    DefaultFontFamily family{DefaultFontFamily::system_ui_sans};
    std::uint32_t logical_pixel_size{14};
    std::uint32_t font_weight{400};
    float line_height{22.0F};
};

struct DefaultTextAliasToken final {
    std::array<float, 4> primary{0.0F, 0.0F, 0.0F, 0.88F};
    std::array<float, 4> secondary{0.0F, 0.0F, 0.0F, 0.65F};
    std::array<float, 4> disabled{0.0F, 0.0F, 0.0F, 0.25F};
};

struct DefaultThemeSnapshot final {
    DefaultTextTypographyToken body;
    DefaultTextAliasToken text;
};

[[nodiscard]] const DefaultThemeSnapshot& default_theme_snapshot() noexcept;

} // namespace ryn::detail
