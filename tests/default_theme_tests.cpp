#include "component/default_theme.hpp"

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
        require(theme == ryn::resolve_theme()
                    && &theme == &ryn::detail::default_theme_snapshot(),
                "Default Theme adapter is not the immutable resolved snapshot");
        require(theme.source_version() == "6.5.0"
                    && theme.source_commit()
                        == "740ad964dc2397f33e40944367b0536a7314cc32",
                "Default Theme source identity drifted");

        const auto& text = theme.text();
        require(text.font_family == ryn::SystemFontFamily::ui_sans
                    && text.font_weight == 400
                    && text.font_size == 14.0F
                    && text.line_height == 22.0F
                    && text.color == ryn::Color(0.0F, 0.0F, 0.0F, 0.88F),
                "Default resolved Text tokens drifted");

        const auto& button = theme.button();
        require(button.control_height_small == 24.0F
                    && button.control_height == 32.0F
                    && button.control_height_large == 40.0F
                    && button.padding_inline_small == 7.0F
                    && button.padding_inline == 15.0F
                    && button.padding_inline_large == 15.0F
                    && button.border_radius_small == 4.0F
                    && button.border_radius == 6.0F
                    && button.border_radius_large == 8.0F,
                "Default resolved Button size tokens drifted");
        require(button.default_color == theme.alias().color_text
                    && button.default_background
                        == theme.alias().color_background_container
                    && button.default_border_color == theme.alias().color_border
                    && button.primary_background == theme.map().color_primary
                    && button.danger_background == theme.map().color_error
                    && button.danger_hover_background
                        == theme.map().color_error_hover
                    && button.danger_active_background
                        == theme.map().color_error_active
                    && button.border_width == 1.0F
                    && button.icon_gap == 8.0F
                    && button.loading_indicator_size == 14.0F
                    && button.loading_opacity == 0.65F,
                "Default resolved Button visual tokens drifted");
        require(theme.alias().line_width_focus == 3.0F
                    && theme.alias().focus_outline_offset == 1.0F
                    && theme.alias().color_focus_outline
                        == theme.map().color_primary_border
                    && theme.map().size_xs == 8.0F
                    && theme.map().size == 16.0F
                    && theme.map().size_large == 24.0F,
                "Default focus or layout gap compatibility drifted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
