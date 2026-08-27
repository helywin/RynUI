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
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
