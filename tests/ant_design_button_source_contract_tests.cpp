#include "theme/theme_runtime_types.hpp"

#include <ryn/theme.hpp>

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::array<std::string_view, 4> source_paths{
    "components/button/style/token.ts",
    "components/button/style/variant.ts",
    "components/style/index.tsx",
    "components/theme/themes/shared/genColorMapToken.ts",
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_locked_source_identity_and_values() {
    const auto theme = ryn::resolve_theme();
    const auto& map = theme.map();
    const auto& alias = theme.alias();
    const auto& button = theme.button();

    require(theme.source_version() == "6.5.0"
                && theme.source_commit()
                    == "740ad964dc2397f33e40944367b0536a7314cc32"
                && source_paths[0] == "components/button/style/token.ts"
                && source_paths[1] == "components/button/style/variant.ts"
                && source_paths[2] == "components/style/index.tsx"
                && source_paths[3]
                    == "components/theme/themes/shared/genColorMapToken.ts",
            "Ant Design Button source identity drifted");

    require(map.color_primary == ryn::Color::rgba8(22, 119, 255)
                && map.color_primary_hover == ryn::Color::rgba8(64, 150, 255)
                && map.color_primary_active == ryn::Color::rgba8(9, 88, 217)
                && map.color_primary_border == ryn::Color::rgba8(145, 202, 255)
                && map.color_error == ryn::Color::rgba8(255, 77, 79)
                && map.color_error_hover == ryn::Color::rgba8(255, 120, 117)
                && map.color_error_active == ryn::Color::rgba8(217, 54, 62),
            "Ant Design semantic Button palette drifted");
    require(alias.line_width_focus == 3.0F
                && alias.focus_outline_offset == 1.0F
                && alias.color_focus_outline == map.color_primary_border,
            "Button focus-visible source contract drifted");

    require(button.default_color == ryn::Color(0.0F, 0.0F, 0.0F, 0.88F)
                && button.default_background == ryn::Color::rgba8(255, 255, 255)
                && button.default_border_color == ryn::Color::rgba8(217, 217, 217)
                && button.default_hover_color == map.color_primary_hover
                && button.default_active_color == map.color_primary_active
                && button.primary_background == map.color_primary
                && button.primary_hover_background == map.color_primary_hover
                && button.primary_active_background == map.color_primary_active
                && button.danger_background == map.color_error
                && button.danger_hover_background == map.color_error_hover
                && button.danger_active_background == map.color_error_active
                && button.disabled_color == ryn::Color(0.0F, 0.0F, 0.0F, 0.25F)
                && button.disabled_background == ryn::Color(0.0F, 0.0F, 0.0F, 0.04F)
                && button.disabled_border_color == ryn::Color::rgba8(217, 217, 217)
                && button.loading_opacity == 0.65F,
            "resolved Button component token identities drifted");
}

void test_custom_error_seed_uses_palette_not_linear_mix() {
    ryn::ThemeConfig config;
    config.seed.color_error = ryn::Color::rgba8(22, 119, 255);
    const auto themed = ryn::resolve_theme(config);
    require(themed.map().color_error == ryn::Color::rgba8(22, 119, 255)
                && themed.map().color_error_hover
                    == ryn::Color::rgba8(64, 150, 255)
                && themed.map().color_error_active
                    == ryn::Color::rgba8(9, 88, 217)
                && themed.button().danger_hover_background
                    == themed.map().color_error_hover
                && themed.button().danger_active_background
                    == themed.map().color_error_active,
            "custom error seed did not use the locked Ant Design palette algorithm");
}

void test_typed_token_identities() {
    using ryn::theme_runtime::TokenIdentity;
    require(ryn::theme_runtime::token_identity_name(
                TokenIdentity::map_color_error_hover) == "map.colorErrorHover"
                && ryn::theme_runtime::token_identity_name(
                    TokenIdentity::map_color_error_active) == "map.colorErrorActive"
                && ryn::theme_runtime::dirty_phase_for(
                    TokenIdentity::map_color_error_hover)
                    == ryn::theme_runtime::DirtyPhase::paint_material
                && ryn::theme_runtime::dirty_phase_for(
                    TokenIdentity::map_color_error_active)
                    == ryn::theme_runtime::DirtyPhase::paint_material,
            "Button palette typed token identities drifted");
}

} // namespace

int main() {
    try {
        test_locked_source_identity_and_values();
        test_custom_error_seed_uses_palette_not_linear_mix();
        test_typed_token_identities();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
