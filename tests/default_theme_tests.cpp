#include "component/default_theme.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        const auto& theme = ryn::detail::default_theme_snapshot();
        require(theme.body.family == ryn::detail::DefaultFontFamily::system_ui_sans
                    && theme.body.logical_pixel_size == 14
                    && theme.body.font_weight == 400
                    && theme.body.line_height == 22.0F,
                "Default Theme body typography drifted from Ant Design 6.5.0");
        require(theme.text.primary == std::array<float, 4>{0.0F, 0.0F, 0.0F, 0.88F}
                    && theme.text.secondary
                        == std::array<float, 4>{0.0F, 0.0F, 0.0F, 0.65F}
                    && theme.text.disabled
                        == std::array<float, 4>{0.0F, 0.0F, 0.0F, 0.25F},
                "Default Theme semantic Text aliases drifted from Ant Design 6.5.0");
        require(&theme == &ryn::detail::default_theme_snapshot(),
                "Default Theme snapshot is not immutable process state");

        const auto& button = theme.button;
        require(button.small.control_height == 24.0F
                    && button.middle.control_height == 32.0F
                    && button.large.control_height == 40.0F
                    && button.small.padding_inline == 7.0F
                    && button.middle.padding_inline == 15.0F
                    && button.large.padding_inline == 15.0F
                    && button.small.border_radius == 4.0F
                    && button.middle.border_radius == 6.0F
                    && button.large.border_radius == 8.0F,
                "Default Theme Button size tokens drifted from Ant Design 6.5.0");
        require(button.small.content_font_size == 14.0F
                    && button.middle.content_font_size == 14.0F
                    && button.large.content_font_size == 16.0F
                    && button.small.content_line_height == 22.0F
                    && button.middle.content_line_height == 22.0F
                    && button.large.content_line_height == 24.0F
                    && button.border_width == 1.0F
                    && button.content_gap == 8.0F,
                "Default Theme Button content tokens drifted from Ant Design 6.5.0");

        using ryn::detail::DefaultColor;
        const DefaultColor white{1.0F, 1.0F, 1.0F, 1.0F};
        const DefaultColor transparent{0.0F, 0.0F, 0.0F, 0.0F};
        const DefaultColor primary{
            22.0F / 255.0F, 119.0F / 255.0F, 1.0F, 1.0F};
        const DefaultColor primary_hover{
            64.0F / 255.0F, 150.0F / 255.0F, 1.0F, 1.0F};
        const DefaultColor primary_active{
            9.0F / 255.0F, 88.0F / 255.0F, 217.0F / 255.0F, 1.0F};
        const DefaultColor default_text{0.0F, 0.0F, 0.0F, 0.88F};
        const DefaultColor default_border{
            217.0F / 255.0F, 217.0F / 255.0F, 217.0F / 255.0F, 1.0F};
        require(button.default_variant.normal.background == white
                    && button.default_variant.normal.border == default_border
                    && button.default_variant.normal.foreground == default_text
                    && button.default_variant.hover
                        == ryn::detail::DefaultButtonVisualStateToken{
                            white, primary_hover, primary_hover}
                    && button.default_variant.active
                        == ryn::detail::DefaultButtonVisualStateToken{
                            white, primary_active, primary_active},
                "Default Button visual state tokens drifted from Ant Design 6.5.0");
        require(button.primary_variant.normal
                        == ryn::detail::DefaultButtonVisualStateToken{
                            primary, transparent, white}
                    && button.primary_variant.hover
                        == ryn::detail::DefaultButtonVisualStateToken{
                            primary_hover, transparent, white}
                    && button.primary_variant.active
                        == ryn::detail::DefaultButtonVisualStateToken{
                            primary_active, transparent, white},
                "Primary Button visual state tokens drifted from Ant Design 6.5.0");
        require(button.disabled.background
                        == DefaultColor{0.0F, 0.0F, 0.0F, 0.04F}
                    && button.disabled.border == default_border
                    && button.disabled.foreground
                        == DefaultColor{0.0F, 0.0F, 0.0F, 0.25F}
                    && button.focus_visible
                        == DefaultColor{
                            145.0F / 255.0F,
                            202.0F / 255.0F,
                            1.0F,
                            1.0F}
                    && button.focus_ring_width == 3.0F
                    && button.focus_ring_offset == 1.0F
                    && button.loading_indicator_size == 14.0F
                    && button.loading_opacity == 0.65F,
                "Button disabled, focus-visible, or loading tokens drifted");
        require(theme.layout_spacing.small == 8.0F
                    && theme.layout_spacing.middle == 16.0F
                    && theme.layout_spacing.large == 24.0F,
                "Default Theme layout spacing drifted from Ant Design 6.5.0");
        require(theme.source.version == "6.5.0"
                    && theme.source.seed_token.ends_with("/6.5.0/components/theme/themes/seed.ts")
                    && theme.source.button_size_style.ends_with(
                        "/6.5.0/components/button/style/index.ts")
                    && theme.source.button_component_token.ends_with(
                        "/6.5.0/components/button/style/token.ts")
                    && theme.source.button_variant_style.ends_with(
                        "/6.5.0/components/button/style/variant.ts")
                    && theme.source.flex_interface.ends_with(
                        "/6.5.0/components/flex/interface.ts")
                    && theme.source.flex_style.ends_with(
                        "/6.5.0/components/flex/style/index.ts")
                    && theme.source.space_component.ends_with(
                        "/6.5.0/components/space/index.tsx")
                    && theme.source.space_style.ends_with(
                        "/6.5.0/components/space/style/index.tsx"),
                "Default Theme component source references are not version-pinned");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
