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

void defaults_and_inheritance() {
    const auto base = resolve_theme();
    const auto& token = base.segmented();
    const auto& m = token.metrics;
    check(m.track_padding == 2 && m.height_small == 24 && m.height == 32 && m.height_large == 40 &&
              m.padding_small == 7 && m.padding == 11 && m.icon_gap == 6 && m.radius_small == 4 && m.radius == 6 &&
              m.radius_large == 8 && m.item_radius_small == 2 && m.item_radius == 4 && m.item_radius_large == 6 &&
              m.font_size == 14 && m.font_size_large == 16,
          "locked Segmented metrics drifted");
    check(token.colors.track == Color::rgba8(245, 245, 245) && token.colors.item == base.alias().color_text_secondary &&
              token.colors.hover == base.alias().color_text &&
              token.colors.selected_background == base.alias().color_background_elevated &&
              token.colors.disabled == base.alias().color_text_disabled && token.effects.focus_width == 3 &&
              token.effects.focus_offset == 1 && token.effects.selected_shadow == base.alias().box_shadow_tertiary,
          "locked Segmented visual recipe drifted");
    ThemeConfig custom;
    custom.segmented.tokens.track = Color::rgba8(20, 40, 70);
    custom.segmented.tokens.height = dp(36);
    const auto parent = resolve_theme(custom);
    check(resolve_theme({}, &parent).segmented() == parent.segmented(), "Segmented overrides did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).segmented() == token, "Segmented inherit=false failed");
    check(parent.drawer() == base.drawer() && parent.popconfirm() == base.popconfirm() && parent.tag() == base.tag() &&
              parent.badge() == base.badge() && parent.alert() == base.alert() && parent.spin() == base.spin() &&
              parent.progress() == base.progress() && parent.tabs() == base.tabs(),
          "Segmented override leaked");
    ThemeConfig independent;
    independent.segmented.algorithm = true;
    independent.segmented.seed.control_height = dp(48);
    check(resolve_theme(independent).segmented().metrics.height == 48 &&
              resolve_theme(independent).button() == base.button(),
          "Segmented component algorithm leaked");
    independent.segmented.algorithm = false;
    check(resolve_theme(independent).segmented() == token, "inactive Segmented seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    const auto night = resolve_theme(dark);
    check(night.segmented().colors.track == Color::rgba8(0, 0, 0) &&
              night.segmented().colors.selected_background == night.alias().color_background_elevated &&
              std::abs(night.segmented().colors.hover_background.alpha() - .12F) < .0001F,
          "Segmented dark absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).segmented().metrics.height == 28, "Segmented compact absent");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"segmented\":") != std::string::npos,
          "Segmented missing snapshot identity/diagnostics");
}

void domains_and_atomic_validation() {
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int effects{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Segmented colors remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->segmented_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::hit_test),
                  "Segmented metric phases incomplete");
            ++metrics;
        },
        [&] { static_cast<void>(scope->segmented_metrics()); });
    auto e = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == (DirtyPhase::geometry | DirtyPhase::paint_material), "Segmented effects remeasured");
            ++effects;
        },
        [&] { static_cast<void>(scope->segmented_effects()); });
    ThemeConfig config;
    config.segmented.tokens.hover = Color::rgba8(40, 60, 80);
    check(scope->update(config) && colors == 1 && !metrics && !effects && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::segmented_colors,
          "Segmented color subscriptions leaked");
    check(!scope->update(config) && colors == 1, "equal Segmented update notified");
    config.segmented.tokens.height = dp(36);
    scope->update(config);
    config.segmented.tokens.focus_width = dp(4);
    scope->update(config);
    check(colors == 1 && metrics == 1 && effects == 1, "Segmented dirty domains drifted");
    const auto current = scope->snapshot();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected{};
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == current, "invalid Segmented partially published");
    };
    auto bad = config;
    bad.segmented.tokens.track_padding = dp(-1);
    reject(bad);
    bad = config;
    bad.segmented.tokens.height = dp(0);
    reject(bad);
    bad = config;
    bad.segmented.tokens.font_size = dp(0);
    reject(bad);
    bad = config;
    bad.segmented.tokens.focus_offset = dp(std::numeric_limits<float>::infinity());
    reject(bad);
    bad = config;
    bad.segmented.tokens.padding = dp(std::numeric_limits<float>::max());
    reject(bad);
    bad = config;
    bad.segmented.tokens.radius = LogicalLength{};
    // Default LogicalLength is auto and must not become a visual length.
    reject(bad);
}
} // namespace

int main() {
    try {
        defaults_and_inheritance();
        domains_and_atomic_validation();
        std::cout << "Segmented Theme contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
