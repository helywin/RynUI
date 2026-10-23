#include "theme/theme_runtime.hpp"
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void defaults_and_isolation() {
    const auto base = resolve_theme();
    const auto& t = base.collapse();
    check(t.metrics.header_block_small == 8 && t.metrics.header_block == 12 && t.metrics.header_block_large == 16 &&
              t.metrics.header_start_small == 8 && t.metrics.header_end_small == 12 &&
              t.metrics.header_start_large == 16 && t.metrics.header_end_large == 24 && t.metrics.body_padding == 16 &&
              t.metrics.arrow_size == 12 && t.metrics.border_radius == 8 && t.typography.font_size == 14 &&
              t.typography.line_height == 22 && t.typography.header_line_height_large == 24 &&
              std::abs(t.colors.header_background.alpha() - .02F) < .0001F,
          "locked Collapse recipe drifted");
    ThemeConfig config;
    config.collapse.tokens.header_background = Color::rgba8(100, 20, 30);
    config.collapse.tokens.header_gap = dp(18);
    const auto changed = resolve_theme(config);
    check(changed.identity() != base.identity() && changed.diagnostic_json().find("\"collapse\":") != std::string::npos,
          "Collapse serialization or identity omitted");
    check(resolve_theme({}, &changed).collapse() == changed.collapse(), "Collapse inheritance lost");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &changed).collapse() == t, "Collapse inherit=false failed");
    check(changed.tabs() == base.tabs() && changed.card() == base.card() && changed.modal() == base.modal() &&
              changed.drawer() == base.drawer() && changed.popconfirm() == base.popconfirm() &&
              changed.tag() == base.tag() && changed.badge() == base.badge() && changed.alert() == base.alert() &&
              changed.spin() == base.spin() && changed.progress() == base.progress() &&
              changed.button() == base.button(),
          "Collapse erased other Theme members");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).collapse().colors.body_background != t.colors.body_background, "Collapse dark absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).collapse().metrics.header_block < t.metrics.header_block, "Collapse compact absent");
    ThemeConfig independent;
    independent.collapse.seed.font_size = dp(20);
    check(resolve_theme(independent).collapse() == t, "inactive Collapse component seed applied");
    independent.collapse.algorithm = true;
    check(resolve_theme(independent).collapse().typography.font_size == 20 &&
              resolve_theme(independent).tabs() == base.tabs(),
          "Collapse component algorithm leaked");
}

void domains_and_atomic_validation() {
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int fonts{};
    int effects{};
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "Collapse color measured");
            ++colors;
        },
        [&] { static_cast<void>(scope->collapse_colors()); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { static_cast<void>(scope->collapse_metrics()); });
    auto f = scope->capture([&](DirtyPhase) { ++fonts; }, [&] { static_cast<void>(scope->collapse_typography()); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { static_cast<void>(scope->collapse_effects()); });
    ThemeConfig config;
    config.collapse.tokens.header_text = Color::rgba8(100, 20, 30);
    check(scope->update(config) && colors == 1 && !metrics && !fonts && !effects &&
              scope->changed_identities().size() == 1,
          "Collapse color domain leaked");
    config.collapse.tokens.header_gap = dp(20);
    scope->update(config);
    config.collapse.tokens.font_size = dp(16);
    scope->update(config);
    config.collapse.tokens.focus_width = dp(4);
    scope->update(config);
    check(colors == 1 && metrics == 1 && fonts == 1 && effects == 1 && !scope->update(config),
          "Collapse subscriptions drifted");
    const auto snapshot = scope->snapshot();
    for (int mode = 0; mode < 5; ++mode) {
        auto invalid = config;
        if (mode == 0) {
            invalid.collapse.tokens.header_block = dp(-1);
        }
        if (mode == 1) {
            invalid.collapse.tokens.font_size = dp(0);
        }
        if (mode == 2) {
            invalid.collapse.tokens.header_gap = dp(std::numeric_limits<float>::infinity());
        }
        if (mode == 3) {
            invalid.collapse.tokens.font_weight = 1001;
        }
        if (mode == 4) {
            invalid.collapse.tokens.header_start = dp(std::numeric_limits<float>::max());
        }
        bool rejected{};
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == snapshot, "invalid Collapse Theme partially published");
    }
}
} // namespace

int main() {
    try {
        defaults_and_isolation();
        domains_and_atomic_validation();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
