#include "theme/theme_runtime.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void defaults_and_inheritance() {
    const auto base = resolve_theme();
    const auto& tabs = base.tabs();
    check(tabs.metrics.line_padding_small == 8 && tabs.metrics.line_padding == 12 &&
              tabs.metrics.line_padding_large == 16 && tabs.metrics.card_height_small == 32 &&
              tabs.metrics.card_height == 40 && tabs.metrics.card_height_large == 48 &&
              tabs.metrics.horizontal_gutter == 32 && tabs.metrics.card_gutter == 2 && tabs.metrics.body_gap == 16 &&
              tabs.metrics.indicator_width == 2 && tabs.metrics.border_radius == 8,
          "locked Tabs metrics drifted");
    check(tabs.typography.font_size_small == 14 && tabs.typography.font_size == 14 &&
              tabs.typography.font_size_large == 16 && tabs.typography.line_height == 22 &&
              tabs.typography.line_height_large == 24 && tabs.effects.focus_width == 3 &&
              tabs.effects.focus_offset == 1 && tabs.colors.indicator == base.map().color_primary &&
              tabs.colors.card_selected_background == base.alias().color_background_container &&
              std::abs(tabs.colors.card_background.alpha() - 0.02F) < 0.0001F,
          "locked Tabs presentation drifted");
    ThemeConfig custom;
    custom.tabs.tokens.indicator = Color::rgba8(100, 20, 50);
    custom.tabs.tokens.horizontal_gutter = dp(40);
    custom.tabs.tokens.font_weight = 600;
    const auto parent = resolve_theme(custom);
    check(resolve_theme({}, &parent).tabs() == parent.tabs(), "Tabs overrides did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).tabs() == tabs, "Tabs inherit reset failed");
    check(parent.card() == base.card() && parent.select() == base.select() && parent.button() == base.button() &&
              parent.typography() == base.typography(),
          "Tabs override leaked to other components");
    ThemeConfig independent;
    independent.tabs.algorithm = true;
    independent.tabs.seed.control_height = dp(48);
    check(resolve_theme(independent).tabs().metrics.card_height == 60 &&
              resolve_theme(independent).card() == base.card(),
          "Tabs component algorithm leaked");
    independent.tabs.algorithm = false;
    check(resolve_theme(independent).tabs() == tabs, "inactive Tabs seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).tabs().colors.card_selected_background != tabs.colors.card_selected_background,
          "Tabs dark theme absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).tabs().metrics.card_height < 40 &&
              resolve_theme(compact).tabs().metrics.horizontal_gutter == 32,
          "Tabs compact/fixed gutter recipe drifted");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"tabs\":") != std::string::npos,
          "Tabs hash/diagnostic serialization omitted");
}

void dirty_domains_and_failures() {
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int fonts{};
    int effects{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Tabs colors remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->tabs_colors()); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { static_cast<void>(scope->tabs_metrics()); });
    auto t = scope->capture([&](DirtyPhase) { ++fonts; }, [&] { static_cast<void>(scope->tabs_typography()); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { static_cast<void>(scope->tabs_effects()); });
    ThemeConfig config;
    config.tabs.tokens.item_hover = Color::rgba8(10, 20, 30);
    check(scope->update(config) && colors == 1 && !metrics && !fonts && !effects &&
              scope->changed_identities().size() == 1 && scope->changed_identities()[0] == TokenIdentity::tabs_colors,
          "Tabs color notifications leaked");
    config.tabs.tokens.body_gap = dp(20);
    scope->update(config);
    config.tabs.tokens.font_size = dp(16);
    scope->update(config);
    config.tabs.tokens.focus_width = dp(4);
    scope->update(config);
    check(colors == 1 && metrics == 1 && fonts == 1 && effects == 1, "Tabs dirty groups drifted");
    check(has_any(dirty_phase_for(TokenIdentity::tabs_metrics), DirtyPhase::measure_layout | DirtyPhase::hit_test) &&
              has_any(dirty_phase_for(TokenIdentity::tabs_typography), DirtyPhase::text | DirtyPhase::measure_layout) &&
              dirty_phase_for(TokenIdentity::tabs_effects) == (DirtyPhase::geometry | DirtyPhase::paint_material),
          "Tabs invalidation phases drifted");
    const auto snapshot = scope->snapshot();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected = false;
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == snapshot, "Tabs invalid config partially published");
    };
    auto invalid = config;
    invalid.tabs.tokens.card_height = dp(0);
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.horizontal_gutter = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.font_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.focus_offset = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.font_weight = 1001;
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.body_gap = dp(std::numeric_limits<float>::infinity());
    reject(invalid);
    invalid = config;
    invalid.tabs.tokens.card_height_large = dp(std::numeric_limits<float>::max());
    reject(invalid);
}
} // namespace

int main() {
    try {
        defaults_and_inheritance();
        dirty_domains_and_failures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
