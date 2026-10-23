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

void defaults_and_overrides() {
    const auto base = resolve_theme();
    const auto& d = base.drawer();
    check(d.metrics == DrawerMetricToken{} && d.typography.font_size == 14 && d.typography.title_font_size == 16 &&
              d.typography.line_height == 22 && d.typography.title_line_height == 24 &&
              d.typography.title_font_weight == 600 && d.colors.background == base.alias().color_background_elevated &&
              d.colors.mask == Color(0, 0, 0, 0.45F) && d.border == base.alias().color_split &&
              d.z_index_popup == 1000 && d.effects.focus_radius == 0,
          "locked Drawer defaults drifted");
    ThemeConfig config;
    config.drawer.tokens.background = Color::rgba8(240, 248, 255);
    config.drawer.tokens.footer_padding_block = dp(12);
    config.drawer.tokens.title_font_weight = 700;
    config.drawer.tokens.z_index_popup = 1500;
    const auto parent = resolve_theme(config);
    check(parent.drawer() == resolve_theme({}, &parent).drawer() && parent.modal() == base.modal() &&
              parent.tabs() == base.tabs() && parent.button() == base.button(),
          "Drawer theme inheritance leaked");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).drawer() == d, "Drawer reset failed");
    ThemeConfig independent;
    independent.drawer.algorithm = true;
    independent.drawer.seed.font_size = dp(18);
    check(resolve_theme(independent).drawer().typography.font_size == 18 &&
              resolve_theme(independent).modal() == base.modal(),
          "Drawer independent algorithm leaked");
    independent.drawer.algorithm = false;
    check(resolve_theme(independent).drawer() == d, "inactive Drawer seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(dark).drawer().colors.background != d.colors.background &&
              resolve_theme(compact).drawer().metrics.padding_inline < d.metrics.padding_inline,
          "Drawer dark/compact recipe failed");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"drawer\":") != std::string::npos &&
              parent.diagnostic_json().find("\"zIndexPopup\":1500") != std::string::npos,
          "Drawer diagnostic/identity omitted tokens");
}

void grouped_updates_and_failures() {
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int fonts{};
    int effects{};
    int order{};
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "colors remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->drawer_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::measure_layout), "metrics phase");
            ++metrics;
        },
        [&] { static_cast<void>(scope->drawer_metrics()); });
    auto f = scope->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text), "font phase");
            ++fonts;
        },
        [&] { static_cast<void>(scope->drawer_typography()); });
    auto e = scope->capture(
        [&](DirtyPhase p) {
            check(p == (DirtyPhase::geometry | DirtyPhase::paint_material), "effects phase");
            ++effects;
        },
        [&] { static_cast<void>(scope->drawer_effects()); });
    auto o = scope->capture(
        [&](DirtyPhase p) {
            check(p == (DirtyPhase::hit_test | DirtyPhase::paint_material), "order phase");
            ++order;
        },
        [&] { static_cast<void>(scope->drawer_order()); });
    ThemeConfig config;
    config.drawer.tokens.border = Color::rgba8(20, 100, 200);
    scope->update(config);
    check(colors == 1 && !metrics && !fonts && !effects && !order && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::drawer_colors,
          "Drawer border invalidation leaked");
    config.drawer.tokens.header_padding_block = dp(20);
    scope->update(config);
    config.drawer.tokens.title_font_size = dp(18);
    scope->update(config);
    config.drawer.tokens.focus_width = dp(4);
    scope->update(config);
    config.drawer.tokens.z_index_popup = 1200;
    scope->update(config);
    check(colors == 1 && metrics == 1 && fonts == 1 && effects == 1 && order == 1 && !scope->update(config),
          "Drawer grouped updates drifted");
    for (auto id : {TokenIdentity::drawer_colors, TokenIdentity::drawer_metrics, TokenIdentity::drawer_typography,
                    TokenIdentity::drawer_effects, TokenIdentity::drawer_order}) {
        check(!token_identity_name(id).empty(), "Drawer identity unnamed");
    }
    const auto previous = scope->snapshot();
    const auto generation = scope->generation();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected{};
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == previous && scope->generation() == generation,
              "Drawer invalid update published");
    };
    auto invalid = config;
    invalid.drawer.tokens.close_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.drawer.tokens.footer_padding_inline = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.drawer.tokens.font_weight = 0;
    reject(invalid);
    invalid = config;
    invalid.drawer.tokens.z_index_popup = -1;
    reject(invalid);
    invalid = config;
    invalid.drawer.tokens.header_padding_inline = dp(std::numeric_limits<float>::quiet_NaN());
    reject(invalid);
    invalid = config;
    invalid.drawer.tokens.padding_block = dp(std::numeric_limits<float>::max());
    reject(invalid);
}
} // namespace

int main() {
    try {
        defaults_and_overrides();
        grouped_updates_and_failures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
