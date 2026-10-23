#include "theme/theme_runtime.hpp"
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

void defaults_and_inheritance() {
    const auto base = resolve_theme();
    const auto& card = base.card();
    check(card.metrics.header_height == 56 && card.metrics.header_height_small == 38 &&
              card.metrics.body_padding == 24 && card.metrics.body_padding_small == 12 &&
              card.metrics.header_padding == 24 && card.metrics.header_padding_small == 12 &&
              card.metrics.actions_padding == 12 && card.metrics.border_radius == 8 && card.metrics.border_width == 1,
          "locked Card metric defaults drifted");
    check(card.typography.header_font_size == 16 && card.typography.header_font_size_small == 14 &&
              card.typography.header_font_weight == 600 && card.typography.line_height == 22 &&
              card.colors.header_background.alpha() == 0 &&
              card.colors.background == base.alias().color_background_container &&
              card.colors.border == base.alias().color_border_secondary &&
              card.effects.shadow == base.alias().box_shadow_tertiary &&
              card.effects.hover_shadow == ant_design_default_shadows().card,
          "locked Card presentation defaults drifted");
    ThemeConfig custom;
    custom.card.tokens.background = Color::rgba8(15, 30, 90);
    custom.card.tokens.body_padding = dp(30);
    custom.card.tokens.header_font_weight = 700;
    const auto parent = resolve_theme(custom);
    check(resolve_theme({}, &parent).card() == parent.card(), "Card override did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).card() == card, "Card inherit=false did not reset");
    check(parent.select() == base.select() && parent.button() == base.button() && parent.tooltip() == base.tooltip() &&
              parent.typography() == base.typography(),
          "Card override changed unrelated tokens");
    ThemeConfig independent;
    independent.card.algorithm = true;
    independent.card.seed.font_size = dp(18);
    check(resolve_theme(independent).card().typography.font_size == 18 &&
              resolve_theme(independent).button() == base.button(),
          "Card component algorithm leaked");
    independent.card.algorithm = false;
    check(resolve_theme(independent).card() == card, "inactive Card component seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).card().colors.background != card.colors.background, "Card dark theme absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).card().metrics.body_padding < card.metrics.body_padding &&
              resolve_theme(compact).card().metrics.body_padding_small == 12,
          "Card compact/fixed small padding drifted");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"card\":") != std::string::npos,
          "Card missing hash/serialization");
}

void groups_and_atomic_failure() {
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int typography{};
    int effects{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Card colors remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->card_colors()); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { static_cast<void>(scope->card_metrics()); });
    auto t = scope->capture([&](DirtyPhase) { ++typography; }, [&] { static_cast<void>(scope->card_typography()); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { static_cast<void>(scope->card_effects()); });
    ThemeConfig config;
    config.card.tokens.header_background = Color::rgba8(10, 20, 30);
    check(scope->update(config) && colors == 1 && metrics == 0 && typography == 0 && effects == 0 &&
              scope->changed_identities().size() == 1 && scope->changed_identities()[0] == TokenIdentity::card_colors,
          "Card color notification leaked");
    config.card.tokens.body_padding = dp(30);
    scope->update(config);
    config.card.tokens.header_font_size = dp(18);
    scope->update(config);
    config.card.tokens.hover_shadow = ShadowList{};
    scope->update(config);
    check(colors == 1 && metrics == 1 && typography == 1 && effects == 1, "Card dirty groups drifted");
    check(has_any(dirty_phase_for(TokenIdentity::card_metrics), DirtyPhase::hit_test | DirtyPhase::measure_layout) &&
              has_any(dirty_phase_for(TokenIdentity::card_typography), DirtyPhase::text | DirtyPhase::measure_layout) &&
              dirty_phase_for(TokenIdentity::card_effects) == (DirtyPhase::geometry | DirtyPhase::paint_material),
          "Card dirty domains drifted");
    const auto before = scope->snapshot();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected = false;
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == before, "Card invalid config partially published");
    };
    auto invalid = config;
    invalid.card.tokens.body_padding = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.card.tokens.header_height = dp(std::numeric_limits<float>::infinity());
    reject(invalid);
    invalid = config;
    invalid.card.tokens.font_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.card.tokens.header_font_weight = 99;
    reject(invalid);
    invalid = config;
    invalid.card.tokens.header_height = dp(std::numeric_limits<float>::max());
    reject(invalid);
}
} // namespace

int main() {
    try {
        defaults_and_inheritance();
        groups_and_atomic_failure();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
