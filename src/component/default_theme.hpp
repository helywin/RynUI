#pragma once

#include <array>
#include <cstdint>
#include <string_view>

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

using DefaultColor = std::array<float, 4>;

struct DefaultButtonSizeToken final {
    float control_height{};
    float padding_inline{};
    float border_radius{};
    float content_font_size{};
    float content_line_height{};

    friend constexpr bool operator==(
        DefaultButtonSizeToken,
        DefaultButtonSizeToken) = default;
};

struct DefaultButtonVisualStateToken final {
    DefaultColor background{};
    DefaultColor border{};
    DefaultColor foreground{};

    friend constexpr bool operator==(
        DefaultButtonVisualStateToken,
        DefaultButtonVisualStateToken) = default;
};

struct DefaultButtonVariantToken final {
    DefaultButtonVisualStateToken normal;
    DefaultButtonVisualStateToken hover;
    DefaultButtonVisualStateToken active;
};

struct DefaultButtonToken final {
    DefaultButtonSizeToken small;
    DefaultButtonSizeToken middle;
    DefaultButtonSizeToken large;
    DefaultButtonVariantToken default_variant;
    DefaultButtonVariantToken primary_variant;
    DefaultButtonVisualStateToken disabled;
    DefaultColor focus_visible;
    float border_width{1.0F};
    float focus_ring_width{3.0F};
    float focus_ring_offset{1.0F};
    float content_gap{8.0F};
    float loading_indicator_size{14.0F};
    float loading_opacity{0.65F};
};

struct DefaultLayoutSpacingToken final {
    float small{8.0F};
    float middle{16.0F};
    float large{24.0F};

    friend constexpr bool operator==(
        DefaultLayoutSpacingToken,
        DefaultLayoutSpacingToken) = default;
};

struct DefaultThemeSourceReference final {
    std::string_view version;
    std::string_view seed_token;
    std::string_view button_size_style;
    std::string_view button_component_token;
    std::string_view button_variant_style;
    std::string_view flex_interface;
    std::string_view flex_style;
    std::string_view space_component;
    std::string_view space_style;
};

struct DefaultThemeSnapshot final {
    DefaultTextTypographyToken body;
    DefaultTextAliasToken text;
    DefaultButtonToken button;
    DefaultLayoutSpacingToken layout_spacing;
    DefaultThemeSourceReference source;
};

[[nodiscard]] const DefaultThemeSnapshot& default_theme_snapshot() noexcept;

} // namespace ryn::detail
