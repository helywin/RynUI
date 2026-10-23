#include "support/input_fixture.hpp"
#include "theme/input_tokens.hpp"

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

void tokens_and_dirty_groups() {
    const auto base = resolve_theme();
    check(base.input_number().control_width == 90 && base.input_number().handle_width == 22 &&
              base.input_number().handle_font_size == 7 &&
              base.input_number().handle_background == base.alias().color_background_container &&
              base.input_number().handle_hover_color == base.map().color_primary,
          "locked numeric tokens drifted");
    ThemeConfig config;
    config.input_number.tokens.handle_background = Color::rgba8(11, 22, 33);
    config.input_number.tokens.handle_width = dp(26);
    config.input.tokens.padding_inline = dp(14);
    const auto parent = resolve_theme(config);
    check(resolve_theme({}, &parent).input_number() == parent.input_number(), "numeric tokens did not inherit");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).input_number() == base.input_number(), "numeric reset did not reset");
    check(parent.button() == base.button() && parent.tag() == base.tag() && parent.select() == base.select() &&
              parent.popconfirm() == base.popconfirm(),
          "numeric tokens leaked to old fields");
    check(parent.identity() != base.identity() &&
              parent.diagnostic_json().find("\"inputNumber\":") != std::string::npos,
          "numeric theme missing identity/serialization");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(dark).input_number().colors() != base.input_number().colors() &&
              resolve_theme(compact).input_number().handle_width < 22,
          "numeric Dark/Compact absent");
    ThemeConfig independent;
    independent.input_number.algorithm = true;
    independent.input_number.seed.font_size = dp(20);
    const auto separate = resolve_theme(independent);
    check(separate.input_number().handle_font_size == 10 && separate.button() == base.button() &&
              detail::derive_input_tokens(separate) == detail::derive_input_tokens(base),
          "numeric algorithm leaked");
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "numeric color remeasures");
            ++colors;
        },
        [&] { static_cast<void>(scope->input_number_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout) && has_any(phase, DirtyPhase::hit_test),
                  "numeric metric domain missing");
            ++metrics;
        },
        [&] { static_cast<void>(scope->input_number_metrics()); });
    ThemeConfig update;
    update.input_number.tokens.handle_background = Color::rgba8(4, 5, 6);
    scope->update(update);
    check(colors == 1 && metrics == 0, "numeric color dirtied metrics");
    update.input_number.tokens.handle_width = dp(28);
    scope->update(update);
    check(colors == 1 && metrics == 1, "numeric metric dirtied colors");
    const auto before = scope->snapshot();
    update.input_number.tokens.handle_font_size = dp(std::numeric_limits<float>::infinity());
    bool rejected{};
    try {
        scope->update(update);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && scope->snapshot() == before, "invalid numeric token partially published");
    check(token_identity_name(TokenIdentity::input_number_colors) == "InputNumber.colors" &&
              token_identity_name(TokenIdentity::input_number_metrics) == "InputNumber.metrics",
          "numeric identity name mismatch");
}

void retained_theme_update() {
    ryn_test::input_component::Fixture f;
    Signal<ThemeConfig> config{ThemeConfig{}};
    f.inputs.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[] { InputNumber(InputNumberProps{}.defaultValue(.25)); }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto actions = f.inputs.number_actions(mounted.component);
    const auto text = f.inputs.text_scene(mounted.component);
    const auto topology = f.nodes.topology_revision();
    const auto measured = f.nodes.require(mounted.node).measure_count;
    const auto action_node = f.services.components().root(actions[0]);
    const auto action_measured = f.nodes.require(action_node).measure_count;
    ThemeConfig next;
    next.input_number.tokens.handle_background = Color::rgba8(40, 50, 60);
    config.set(next);
    f.synchronize();
    check(f.nodes.require(mounted.node).measure_count == measured &&
              f.nodes.require(action_node).measure_count == action_measured,
          "numeric pure color update remeasured");
    next.input_number.tokens.control_width = dp(150);
    next.input_number.tokens.handle_width = dp(30);
    config.set(next);
    f.synchronize();
    check(f.nodes.require(mounted.node).bounds.width == 150 && f.nodes.require(action_node).bounds.width == 30,
          "numeric metric override did not change retained geometry");
    check(f.inputs.number_actions(mounted.component) == actions && f.inputs.text_scene(mounted.component) == text &&
              f.nodes.topology_revision() == topology,
          "numeric token update remounted topology");
}
} // namespace

int main() {
    try {
        tokens_and_dirty_groups();
        retained_theme_update();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
