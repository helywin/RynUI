#include "theme/theme_runtime.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void tokens_and_inheritance() {
    const auto base = resolve_theme();
    const auto& token = base.select();
    check(token.metrics.option_height == 32 && token.metrics.option_padding_inline == 12 &&
              token.metrics.option_padding_block == 5 && token.metrics.popup_padding == 4 &&
              token.typography.font_size == 14 && token.typography.line_height == 22 &&
              token.typography.selected_font_weight == 600 && token.z_index_popup == 1050,
          "locked Select defaults drifted");
    check(token.colors.option_selected_background == Color::rgba8(230, 244, 255) &&
              token.colors.selector_background == base.alias().color_background_container &&
              token.colors.option_selected_color == base.alias().color_text,
          "Select default color recipe");
    ThemeConfig custom;
    custom.select.tokens.option_selected_background = Color::rgba8(20, 50, 90);
    custom.select.tokens.option_height = dp(40);
    custom.select.tokens.selected_font_weight = 700;
    custom.select.tokens.z_index_popup = 1100;
    const auto parent = resolve_theme(custom);
    check(resolve_theme({}, &parent).select() == parent.select(), "Select inheritance");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).select() == token, "Select inherit reset");
    check(parent.button() == base.button() && parent.tooltip() == base.tooltip() &&
              parent.typography() == base.typography() && parent.slider() == base.slider(),
          "Select override leaked");
    ThemeConfig algorithm;
    algorithm.select.algorithm = true;
    algorithm.select.seed.control_height = dp(48);
    const auto derived = resolve_theme(algorithm);
    check(derived.select().metrics.option_height == 48 && derived.button() == base.button(),
          "Select component algorithm");
    algorithm.select.algorithm = false;
    check(resolve_theme(algorithm).select() == token, "inactive component seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).select().colors.popup_background != token.colors.popup_background, "dark Select absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).select().metrics.option_height < 32, "compact Select absent");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"select\":") != std::string::npos,
          "Select hash or serialization omitted");
}

void groups_and_failures() {
    auto scope = ThemeScope::create_default();
    int colors = 0;
    int typography = 0;
    int metrics = 0;
    int effects = 0;
    int order = 0;
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Select colors remeasure");
            ++colors;
        },
        [&] { static_cast<void>(scope->select_colors()); });
    auto t = scope->capture([&](DirtyPhase) { ++typography; }, [&] { static_cast<void>(scope->select_typography()); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { static_cast<void>(scope->select_metrics()); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { static_cast<void>(scope->select_effects()); });
    auto o = scope->capture([&](DirtyPhase) { ++order; }, [&] { static_cast<void>(scope->select_order()); });
    ThemeConfig config;
    config.select.tokens.option_active_background = Color::rgba8(10, 20, 30);
    check(scope->update(config) && colors == 1 && !typography && !metrics && !effects && !order,
          "Select color notification leaked");
    check(scope->changed_identities().size() == 1 && scope->changed_identities()[0] == TokenIdentity::select_colors,
          "Select changed identity");
    config.select.tokens.option_height = dp(40);
    scope->update(config);
    config.select.tokens.font_size = dp(18);
    scope->update(config);
    config.select.tokens.shadow = ShadowList{};
    scope->update(config);
    config.select.tokens.z_index_popup = 1200;
    scope->update(config);
    check(colors == 1 && metrics == 1 && typography == 1 && effects == 1 && order == 1, "Select Token groups");
    check(has_any(dirty_phase_for(TokenIdentity::select_typography), DirtyPhase::text | DirtyPhase::measure_layout) &&
              has_any(dirty_phase_for(TokenIdentity::select_metrics),
                      DirtyPhase::hit_test | DirtyPhase::measure_layout) &&
              dirty_phase_for(TokenIdentity::select_order) == DirtyPhase::paint_material,
          "Select dirty domains");
    const auto identity = scope->snapshot().identity();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected = false;
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot().identity() == identity, "invalid Select Token partially published");
    };
    auto invalid = config;
    invalid.select.tokens.option_height = dp(0);
    reject(invalid);
    invalid = config;
    invalid.select.tokens.font_size = auto_length;
    reject(invalid);
    invalid = config;
    invalid.select.tokens.selected_font_weight = 10;
    reject(invalid);
    invalid = config;
    invalid.seed.z_index_popup_base = std::numeric_limits<std::int32_t>::max();
    reject(invalid);
    check(token_identity_name(TokenIdentity::select_effects) == "Select.effects", "Select identity name");
}
} // namespace

int main() {
    try {
        tokens_and_inheritance();
        groups_and_failures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
