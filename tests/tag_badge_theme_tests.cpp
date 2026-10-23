#include "theme/theme_runtime.hpp"
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

void defaults_inheritance_and_invalidation() {
    const auto base = resolve_theme();
    check(base.tag().metrics.font_size == 12 && base.tag().metrics.line_height == 20 &&
              base.tag().metrics.padding_inline == 7 && base.tag().metrics.border_width == 1 &&
              base.tag().metrics.close_size == 10,
          "Tag locked metrics differ");
    check(base.badge().metrics.indicator_height == 20 && base.badge().metrics.indicator_height_small == 14 &&
              base.badge().metrics.dot_size == 6 && base.badge().metrics.status_size == 6 &&
              base.badge().metrics.padding_inline == 8,
          "Badge locked metrics differ");
    check(base.tag().colors.normal.background == Color::rgba8(245, 245, 245) &&
              base.tag().colors.presets[9].background == Color::rgba8(230, 244, 255) &&
              base.tag().colors.presets[9].border == Color::rgba8(145, 202, 255),
          "Tag default/preset palette differs");
    check(base.badge().colors.background == base.map().color_error &&
              base.badge().colors.statuses[2] == base.map().color_success,
          "Badge semantic colors differ");
    ThemeConfig config;
    config.tag.tokens.default_color = Color::rgba8(12, 24, 48);
    config.badge.tokens.background = Color::rgba8(48, 24, 12);
    const auto parent = resolve_theme(config);
    check(resolve_theme({}, &parent).tag() == parent.tag() && resolve_theme({}, &parent).badge() == parent.badge(),
          "status tokens did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).tag() == base.tag() && resolve_theme(reset, &parent).badge() == base.badge(),
          "status tokens did not reset");
    check(parent.button() == base.button() && parent.card() == base.card(), "status override leaked");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"badge\":") != std::string::npos,
          "status tokens missing identity/serialization");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).tag().colors != base.tag().colors &&
              resolve_theme(dark).badge().colors != base.badge().colors,
          "dark status colors absent");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).badge().metrics.indicator_height < 20 &&
              resolve_theme(compact).tag().metrics.font_size < 12,
          "compact metrics absent");
    ThemeConfig independent;
    independent.tag.algorithm = true;
    independent.tag.seed.font_size = dp(18);
    independent.badge.algorithm = true;
    independent.badge.seed.font_size = dp(18);
    check(resolve_theme(independent).tag().metrics.font_size == 14 &&
              resolve_theme(independent).badge().metrics.indicator_height == 24 &&
              resolve_theme(independent).button() == base.button(),
          "component algorithm leaked");
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "status color remeasures");
            ++colors;
        },
        [&] {
            static_cast<void>(scope->tag_colors());
            static_cast<void>(scope->badge_colors());
        });
    auto m = scope->capture([&](auto) { ++metrics; },
                            [&] {
                                static_cast<void>(scope->tag_metrics());
                                static_cast<void>(scope->badge_metrics());
                            });
    scope->update(config);
    check(colors == 1 && metrics == 0, "status colors dirty metrics");
    config.tag.tokens.padding_inline = dp(12);
    config.badge.tokens.indicator_height = dp(24);
    scope->update(config);
    check(colors == 1 && metrics == 1, "status metrics dirty colors");
    const auto before = scope->snapshot();
    for (int i = 0; i < 4; ++i) {
        auto invalid = config;
        if (i == 0) {
            invalid.tag.tokens.font_size = dp(0);
        }
        if (i == 1) {
            invalid.badge.tokens.indicator_height = dp(-1);
        }
        if (i == 2) {
            invalid.badge.tokens.font_weight = 99;
        }
        if (i == 3) {
            invalid.tag.tokens.padding_inline = dp(std::numeric_limits<float>::max());
            invalid.tag.tokens.icon_gap = dp(std::numeric_limits<float>::max());
        }
        bool rejected = false;
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == before, "invalid status tokens partially published");
    }
}
} // namespace

int main() {
    try {
        defaults_inheritance_and_invalidation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
