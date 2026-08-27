#include "component/default_theme.hpp"

namespace ryn::detail {
namespace {

[[nodiscard]] constexpr DefaultColor rgb(
    std::uint8_t red,
    std::uint8_t green,
    std::uint8_t blue,
    float alpha = 1.0F) noexcept {
    constexpr float channel_max = 255.0F;
    return {
        static_cast<float>(red) / channel_max,
        static_cast<float>(green) / channel_max,
        static_cast<float>(blue) / channel_max,
        alpha,
    };
}

constexpr DefaultColor transparent{0.0F, 0.0F, 0.0F, 0.0F};
constexpr DefaultColor white = rgb(255, 255, 255);
constexpr DefaultColor primary = rgb(22, 119, 255);
constexpr DefaultColor primary_hover = rgb(64, 150, 255);
constexpr DefaultColor primary_active = rgb(9, 88, 217);
constexpr DefaultColor default_text{0.0F, 0.0F, 0.0F, 0.88F};
constexpr DefaultColor default_border = rgb(217, 217, 217);
constexpr DefaultColor disabled_background{0.0F, 0.0F, 0.0F, 0.04F};
constexpr DefaultColor disabled_text{0.0F, 0.0F, 0.0F, 0.25F};
constexpr DefaultColor focus_visible = rgb(145, 202, 255);

constexpr DefaultThemeSnapshot snapshot{
    .body = {},
    .text = {},
    .button = {
        .small = {24.0F, 7.0F, 4.0F, 14.0F, 22.0F},
        .middle = {32.0F, 15.0F, 6.0F, 14.0F, 22.0F},
        .large = {40.0F, 15.0F, 8.0F, 16.0F, 24.0F},
        .default_variant = {
            .normal = {white, default_border, default_text},
            .hover = {white, primary_hover, primary_hover},
            .active = {white, primary_active, primary_active},
        },
        .primary_variant = {
            .normal = {primary, transparent, white},
            .hover = {primary_hover, transparent, white},
            .active = {primary_active, transparent, white},
        },
        .disabled = {disabled_background, default_border, disabled_text},
        .focus_visible = focus_visible,
    },
    .source = {
        .version = "6.5.0",
        .seed_token = "https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/theme/themes/seed.ts",
        .button_size_style = "https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/index.ts",
        .button_component_token = "https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/token.ts",
        .button_variant_style = "https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/variant.ts",
    },
};

} // namespace

const DefaultThemeSnapshot& default_theme_snapshot() noexcept {
    return snapshot;
}

} // namespace ryn::detail
