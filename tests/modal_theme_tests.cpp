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
    const auto& modal = base.modal();
    check(modal.metrics.padding_inline == 24 && modal.metrics.padding_block == 20 && modal.metrics.header_gap == 8 &&
              modal.metrics.footer_gap == 12 && modal.metrics.button_gap == 8 && modal.metrics.border_radius == 8 &&
              modal.metrics.close_size == 32 && modal.metrics.close_icon_size == 16 &&
              modal.metrics.close_inset == 12 && modal.metrics.viewport_margin == 16 &&
              modal.metrics.top_offset == 100 && modal.z_index_popup == 1000,
          "locked Modal metrics drifted");
    check(modal.typography.font_size == 14 && modal.typography.line_height == 22 &&
              modal.typography.title_font_size == 16 && modal.typography.title_line_height == 24 &&
              modal.typography.font_weight == 400 && modal.typography.title_font_weight == 600 &&
              modal.colors.background == base.alias().color_background_elevated &&
              modal.colors.title == base.alias().color_text && modal.colors.mask == Color(0, 0, 0, 0.45F) &&
              modal.colors.header_background.alpha() == 0 && modal.colors.footer_background.alpha() == 0 &&
              modal.effects.shadow == base.alias().box_shadow && modal.effects.focus_width == 3,
          "locked Modal presentation drifted");
    ThemeConfig custom;
    custom.modal.tokens.background = Color::rgba8(240, 250, 255);
    custom.modal.tokens.padding_inline = dp(30);
    custom.modal.tokens.title_font_weight = 700;
    custom.modal.tokens.z_index_popup = 1400;
    const auto parent = resolve_theme(custom);
    check(resolve_theme({}, &parent).modal() == parent.modal(), "Modal component override did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).modal() == modal, "Modal inheritance reset failed");
    check(parent.button() == base.button() && parent.tabs() == base.tabs() && parent.card() == base.card() &&
              parent.select() == base.select() && parent.tooltip() == base.tooltip(),
          "Modal override leaked to other components");
    ThemeConfig independent;
    independent.modal.algorithm = true;
    independent.modal.seed.control_height = dp(48);
    independent.modal.seed.color_primary = Color::rgba8(114, 46, 209);
    const auto component = resolve_theme(independent);
    check(component.modal().metrics.close_size == 48 &&
              component.modal().colors.focus_outline != modal.colors.focus_outline &&
              component.button() == base.button() && component.tabs() == base.tabs(),
          "Modal component algorithm leaked");
    independent.modal.algorithm = false;
    check(resolve_theme(independent).modal() == modal, "inactive Modal seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).modal().colors.background != modal.colors.background &&
              resolve_theme(dark).modal().colors.mask == modal.colors.mask,
          "Modal dark theme/mask recipe drifted");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).modal().metrics.close_size < 32 &&
              resolve_theme(compact).modal().metrics.top_offset == 100,
          "Modal compact/fixed top recipe drifted");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"modal\":") != std::string::npos &&
              parent.diagnostic_json().find("\"zIndexPopup\":1400") != std::string::npos,
          "Modal identity/diagnostic omitted tokens");
}

void precise_domains_and_atomic_failures() {
    auto scope = ThemeScope::create_default();
    int colors{}, metrics{}, fonts{}, effects{}, order{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Modal colors remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->modal_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout), "Modal metrics phase missing");
            ++metrics;
        },
        [&] { static_cast<void>(scope->modal_metrics()); });
    auto t = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::text), "Modal font phase missing");
            ++fonts;
        },
        [&] { static_cast<void>(scope->modal_typography()); });
    auto e = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == (DirtyPhase::geometry | DirtyPhase::paint_material), "Modal effect phase drifted");
            ++effects;
        },
        [&] { static_cast<void>(scope->modal_effects()); });
    auto o = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == (DirtyPhase::hit_test | DirtyPhase::paint_material), "Modal order remeasured");
            ++order;
        },
        [&] { static_cast<void>(scope->modal_order()); });
    ThemeConfig config;
    config.modal.tokens.mask = Color(0, 0, 0, 0.6F);
    check(scope->update(config) && colors == 1 && !metrics && !fonts && !effects && !order &&
              scope->changed_identities().size() == 1 && scope->changed_identities()[0] == TokenIdentity::modal_colors,
          "Modal color invalidation leaked");
    config.modal.tokens.padding_block = dp(24);
    scope->update(config);
    config.modal.tokens.title_font_size = dp(18);
    scope->update(config);
    config.modal.tokens.focus_width = dp(4);
    scope->update(config);
    config.modal.tokens.z_index_popup = 1200;
    scope->update(config);
    check(colors == 1 && metrics == 1 && fonts == 1 && effects == 1 && order == 1 && !scope->update(config),
          "Modal grouped/unchanged invalidation drifted");
    for (auto identity : {TokenIdentity::modal_colors, TokenIdentity::modal_metrics, TokenIdentity::modal_typography,
                          TokenIdentity::modal_effects, TokenIdentity::modal_order}) {
        check(!token_identity_name(identity).empty(), "Modal token identity has no name");
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
              "Modal invalid configuration partially published");
    };
    auto invalid = config;
    invalid.modal.tokens.close_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.padding_inline = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.title_font_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.font_weight = 99;
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.title_font_weight = 1001;
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.focus_offset = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.z_index_popup = -1;
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.top_offset = dp(std::numeric_limits<float>::quiet_NaN());
    reject(invalid);
    invalid = config;
    invalid.modal.tokens.padding_block = dp(std::numeric_limits<float>::max());
    reject(invalid);
    config.modal.tokens.focus_width = dp(0);
    config.modal.tokens.border_radius = dp(0);
    check(scope->update(config), "Modal rejected zero optional effects");
}
} // namespace

int main() {
    try {
        defaults_and_inheritance();
        precise_domains_and_atomic_failures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
