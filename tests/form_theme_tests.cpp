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
              parent.segmented() == base.segmented() && parent.modal() == base.modal() &&
              parent.empty() == base.empty() && parent.result() == base.result(),
          "Form override leaked");
    ThemeConfig local;
    local.form.algorithm = true;
    local.form.seed.font_size = dp(18);
    const auto independent = resolve_theme(local);
    check(independent.form().metrics.font_size == 18 && independent.text() == base.text() &&
              independent.empty() == base.empty() && independent.result() == base.result(),
          "Form local algorithm leaked");
    ThemeConfig presentation;
    presentation.empty.tokens.description = Color::rgba8(20, 60, 100);
    presentation.result.algorithm = true;
    presentation.result.seed.font_size = dp(20);
    check(resolve_theme(presentation).form() == form, "Empty/Result overrides leaked into Form");
    local.form.algorithm = false;
    check(resolve_theme(local).form() == form, "inactive Form seed applied");
    check(resolve_theme(local, &parent).form() == parent.form(),
          "inactive Form seed discarded inherited parent overrides");
    local.form.tokens.help = Color::rgba8(90, 50, 10);
    const auto child_override = resolve_theme(local, &parent);
    check(child_override.form().colors.help == *local.form.tokens.help &&
              child_override.form().colors.label == parent.form().colors.label &&
              child_override.form().metrics == parent.form().metrics,
          "child Form override discarded other inherited tokens with an inactive seed");
    local.form.algorithm = true;
    check(resolve_theme(local, &parent).form().metrics.font_size == 18,
          "enabling local Form algorithm failed to apply its seed");
    local.form.algorithm = false;
    local.inherit = false;
    check(resolve_theme(local, &parent).form().metrics == form.metrics,
          "inherit=false applied inactive Form seed or retained parent metrics");
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

void inactive_seed_reactive_inheritance() {
    ThemeConfig parent_config;
    parent_config.form.tokens.label = Color::rgba8(20, 60, 100);
    parent_config.form.tokens.font_size = dp(19);
    const auto parent = ThemeScope::create_default();
    parent->update(parent_config);
    ThemeConfig child_config;
    child_config.form.seed.font_size = dp(24);
    child_config.form.seed.color_primary = Color::rgba8(100, 60, 20);
    const auto child = ThemeScope::create(parent, child_config);
    check(child->snapshot().form() == parent->snapshot().form(),
          "reactive child lost parent Form overrides with an inactive seed");
    int notifications{};
    auto subscription = child->capture([&](DirtyPhase) { ++notifications; },
                                       [&] {
                                           static_cast<void>(child->form_colors());
                                           static_cast<void>(child->form_metrics());
                                       });
    const auto* previous = &child->snapshot();
    child_config.form.seed.font_size = dp(28);
    child->update(child_config);
    check(&child->snapshot() == previous && notifications == 0,
          "inactive Form seed update changed the snapshot or notified subscribers");
    parent_config.form.tokens.help = Color::rgba8(70, 30, 110);
    parent->update(parent_config);
    check(child->snapshot().form() == parent->snapshot().form() && notifications == 1,
          "parent Form update did not propagate past an inactive local seed");
    child_config.form.algorithm = true;
    child->update(child_config);
    check(child->snapshot().form().metrics.font_size == 28 && notifications == 2,
          "reactive Form algorithm did not enable its local seed");
    child_config.form.algorithm = false;
    child->update(child_config);
    check(child->snapshot().form() == parent->snapshot().form() && notifications == 3,
          "disabling Form algorithm did not restore parent overrides");
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

void logical_font_representation() {
    for (bool algorithm : {false, true}) {
        ThemeConfig config;
        config.form.algorithm = algorithm;
        config.form.seed.font_size = dp(18);
        for (float size : {.49F, .1F, std::numeric_limits<float>::denorm_min(), std::nextafter(.5F, 0.0F),
                           std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            config.form.tokens.font_size = dp(size);
            bool rejected{};
            try {
                static_cast<void>(resolve_theme(config));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected, "Form resolver accepted a font that rounds to zero logical pixels");
        }
        for (float size : {.5F, std::nextafter(.5F, 1.0F), 1.0F, 14.0F}) {
            config.form.tokens.font_size = dp(size);
            check(resolve_theme(config).form().metrics.font_size == size,
                  "representable Form font was changed/rejected");
        }
    }
    ThemeConfig derived;
    derived.form.algorithm = true;
    derived.form.seed.font_size = dp(.49F);
    bool rejected{};
    try {
        static_cast<void>(resolve_theme(derived));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "Form component algorithm accepted a final unrepresentable font");
    derived.form.seed.font_size = dp(.5F);
    check(resolve_theme(derived).form().metrics.font_size == 1, "Form component seed rounding changed");
}
} // namespace

int main() {
    try {
        defaults_inheritance_and_local_algorithm();
        inactive_seed_reactive_inheritance();
        dirty_domains_and_atomic_rejection();
        logical_font_representation();
        std::cout << "Form Theme contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
