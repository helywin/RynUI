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

void defaults_inheritance_and_local_algorithm() {
    const auto base = resolve_theme();
    const auto& form = base.form();
    check(form.colors.label == base.alias().color_text && form.colors.required == base.map().color_error &&
              form.colors.error == base.map().color_error && form.colors.help == Color(0, 0, 0, .45F) &&
              form.metrics.item_margin_bottom == 24 && form.metrics.label_gap == 8 && form.metrics.required_gap == 4 &&
              form.metrics.font_size == 14 && form.metrics.line_height == 22,
          "locked Form defaults drifted");
    ThemeConfig custom;
    custom.form.tokens.label = Color::rgba8(20, 60, 100);
    custom.form.tokens.label_gap = dp(12);
    const auto parent = resolve_theme(custom);
    check(parent.form() != form && parent != base && parent.identity() != base.identity() &&
              parent.diagnostic_json().find("\"form\":") != std::string::npos,
          "Form equality/identity/serialization missing");
    check(resolve_theme({}, &parent).form() == parent.form(), "Form override did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).form() == form, "Form inherit=false failed");
    check(parent.button() == base.button() && parent.input_number() == base.input_number() &&
              parent.segmented() == base.segmented() && parent.modal() == base.modal(),
          "Form override leaked");
    ThemeConfig local;
    local.form.algorithm = true;
    local.form.seed.font_size = dp(18);
    const auto independent = resolve_theme(local);
    check(independent.form().metrics.font_size == 18 && independent.text() == base.text(),
          "Form local algorithm leaked");
    local.form.algorithm = false;
    check(resolve_theme(local).form() == form, "inactive Form seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    const auto night = resolve_theme(dark);
    check(night.form().colors.label == night.alias().color_text && night.form().colors.help.red() == 1 &&
              night.form().colors.required == night.map().color_error,
          "Form Dark recipe missing");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).form().metrics.font_size == resolve_theme(compact).map().font_size &&
              resolve_theme(compact).form().metrics.item_margin_bottom < form.metrics.item_margin_bottom,
          "Form Compact recipe missing");
}

void dirty_domains_and_atomic_rejection() {
    const auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Form color update remeasured");
            ++colors;
        },
        [&] { static_cast<void>(scope->form_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::hit_test),
                  "Form metric phases incomplete");
            ++metrics;
        },
        [&] { static_cast<void>(scope->form_metrics()); });
    ThemeConfig config;
    config.form.tokens.help = Color::rgba8(20, 50, 90);
    check(scope->update(config) && colors == 1 && metrics == 0 && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::form_colors,
          "Form color subscriptions leaked");
    check(!scope->update(config) && colors == 1, "equal Form update notified");
    config.form.tokens.font_size = dp(18);
    scope->update(config);
    check(colors == 1 && metrics == 1, "Form metrics subscription missing");
    const auto current = scope->snapshot();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected{};
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == current && colors == 1 && metrics == 1,
              "invalid Form token partially published");
    };
    auto invalid = config;
    invalid.form.tokens.label_gap = dp(-1);
    reject(invalid);
    invalid = config;
    invalid.form.tokens.font_size = dp(0);
    reject(invalid);
    invalid = config;
    invalid.form.tokens.line_height = dp(std::numeric_limits<float>::infinity());
    reject(invalid);
    invalid = config;
    invalid.form.tokens.item_margin_bottom = dp(std::numeric_limits<float>::max());
    reject(invalid);
    invalid = config;
    invalid.form.tokens.required_gap = LogicalLength{};
    reject(invalid);
}
} // namespace

int main() {
    try {
        defaults_inheritance_and_local_algorithm();
        dirty_domains_and_atomic_rejection();
        std::cout << "Form Theme contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
