#include "support/input_fixture.hpp"
#include "component/space_compact.hpp"
#include "theme/input_tokens.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.001F;
}

void variant_matrix() {
    for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark}) {
        for (const auto variant :
             {InputVariant::Outlined, InputVariant::Borderless, InputVariant::Filled, InputVariant::Underlined}) {
            for (const auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
                for (const auto status : {InputStatus::Default, InputStatus::Warning, InputStatus::Error}) {
                    Fixture f;
                    Signal<bool> disabled{false};
                    ThemeConfig config;
                    config.algorithms = {algorithm};
                    InputRef reference;
                    f.inputs.mount(Content{[&] {
                        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                                  Input(InputProps{}
                                            .defaultValue(u8"abc")
                                            .variant(variant)
                                            .size(size)
                                            .status(status)
                                            .disabled(disabled)
                                            .ref(reference));
                              }});
                    }});
                    f.synchronize();
                    const auto mounted = f.inputs.mounted_inputs().front();
                    const auto root = f.nodes.require(mounted.node).bounds;
                    const auto tokens = detail::derive_input_tokens(resolve_theme(config));
                    const auto& colors = tokens.colors;
                    const auto effects = [&] {
                        return f.buttons.rounded_effects().packed_instances();
                    };
                    const auto border = [&] {
                        return effects()[detail::input_border_layer];
                    };
                    const auto background = [&] {
                        return effects()[detail::input_background_layer];
                    };
                    require(near(root.height, tokens.size(size).control_height), "variant changed the control height");
                    if (variant == InputVariant::Borderless) {
                        require(background().material.color.alpha() == 0 && border().material.opacity == 0,
                                "Borderless painted background or border");
                    } else if (variant == InputVariant::Underlined) {
                        require(near(border().geometry.shape.rect.height, tokens.border_width) &&
                                    near(border().geometry.shape.rect.y, root.y + root.height - tokens.border_width) &&
                                    border().geometry.shape.radius == 0,
                                "Underlined is not an isolated square bottom edge");
                        require(graphics::rounded_effect_coverage({root.x + root.width / 2, root.y + 4}, border(), 1) ==
                                    0,
                                "Underlined painted the top or interior");
                    } else if (variant == InputVariant::Filled) {
                        const auto expected = status == InputStatus::Error     ? colors.error_background
                                              : status == InputStatus::Warning ? colors.warning_background
                                                                               : colors.filled_background;
                        require(background().material.color == expected && border().material.color.alpha() == 0 &&
                                    border().geometry.kind == graphics::RoundedEffectKind::outline,
                                "Filled idle colors or border geometry incorrect");
                        const runtime::Point interior{root.x + root.width / 2, root.y + root.height / 2};
                        require(graphics::rounded_effect_coverage(interior, border(), 1) == 0 &&
                                    graphics::rounded_effect_coverage(interior, background(), 1) == 1,
                                "Filled double-painted its translucent interior");
                    }
                    for (const auto modality : {input::FocusModality::pointer, input::FocusModality::keyboard}) {
                        require(f.buttons.focus().request_focus(mounted.interaction, modality), "variant focus failed");
                        f.synchronize();
                        const auto outline = effects()[detail::input_focus_layer];
                        require(outline.material.opacity ==
                                    (variant == InputVariant::Borderless &&
                                             modality == input::FocusModality::keyboard && tokens.focus_width > 0
                                         ? 1
                                         : 0),
                                "variant focus-visible policy incorrect");
                        if (variant != InputVariant::Outlined) {
                            require(effects()[detail::input_shadow_layer_capacity - 1].material.opacity == 0,
                                    "non-Outlined variant retained active shadow");
                        }
                        if (variant == InputVariant::Filled) {
                            require(background().material.color == colors.active_background &&
                                        border().material.color.alpha() > 0,
                                    "Filled focus did not use active background/border");
                        }
                        require(reference.blur(), "variant blur failed");
                        f.synchronize();
                    }
                    disabled.set(true);
                    f.synchronize();
                    require(!reference.focus() && effects()[detail::input_focus_layer].material.opacity == 0,
                            "disabled variant remained focusable");
                    if (variant == InputVariant::Underlined) {
                        require(background().material.color == colors.background,
                                "disabled Underlined used Filled background");
                    } else if (variant == InputVariant::Filled) {
                        require(background().material.color == colors.disabled_background &&
                                    border().material.color == colors.disabled_border,
                                "disabled Filled incorrectly inherited validation colors");
                    }
                }
            }
        }
    }
}

void retained_switches_and_tokens() {
    Fixture f;
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    InputRef reference;
    int parent_runs{};
    int slot_runs{};
    f.inputs.mount(Content{[&] {
        ++parent_runs;
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Input(InputProps{}.defaultValue(u8"hello中").variant(variant).ref(reference), InputPrefix{[&] {
                            ++slot_runs;
                            Text(u8"P");
                        }});
              }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto scene = f.inputs.text_scene(mounted.component);
    require(reference.focus({InputFocusCursor::All}), "retained focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
            "variant preedit failed");
    f.synchronize();
    const auto effect_count = f.buttons.rounded_effects().live_count();
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    const auto selection = f.inputs.editors().require(mounted.editor).selection();
    for (const auto candidate :
         {InputVariant::Filled, InputVariant::Underlined, InputVariant::Borderless, InputVariant::Outlined}) {
        variant.set(candidate);
        f.synchronize();
        require(f.inputs.text_scene(mounted.component) == scene && f.inputs.sessions().active() == stamp &&
                    f.inputs.editors().require(mounted.editor).selection() == selection &&
                    f.inputs.editors().require(mounted.editor).composition().active &&
                    f.buttons.rounded_effects().live_count() == effect_count &&
                    f.scene.text_state(scene).counters().shape_count == shapes && parent_runs == 1 && slot_runs == 1,
                "variant switch rebuilt or disturbed editor/IME/slots");
    }
    bool rejected{};
    try {
        variant.set(static_cast<InputVariant>(255));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && f.inputs.sessions().active() == stamp, "invalid variant mutated editor");
    variant.set(InputVariant::Filled);
    require(reference.blur(), "token fixture blur failed");
    f.synchronize();
    const auto measurements = f.nodes.require(mounted.node).measure_count;
    ThemeConfig config;
    config.input.tokens.filled_background = Color::rgba8(40, 80, 120, 77);
    config.input.tokens.filled_hover_background = Color::rgba8(20, 60, 100, 99);
    config.input.tokens.focus_width = dp(5);
    theme.set(config);
    f.synchronize();
    require(f.buttons.rounded_effects().packed_instances()[detail::input_background_layer].material.color ==
                    *config.input.tokens.filled_background &&
                f.nodes.require(mounted.node).measure_count == measurements,
            "Filled token update lost alpha or measured unchanged text");
    variant.set(InputVariant::Borderless);
    require(reference.focus(), "borderless token focus failed");
    f.synchronize();
    require(f.buttons.rounded_effects().packed_instances()[detail::input_focus_layer].geometry.outline_width == 5,
            "focus width override did not invalidate geometry");
    config.input.tokens.focus_width = dp(0);
    theme.set(config);
    f.synchronize();
    require(f.buttons.rounded_effects().packed_instances()[detail::input_focus_layer].material.opacity == 0,
            "zero focus width retained outline");
    const auto light = detail::derive_input_tokens(resolve_theme());
    ThemeConfig dark_config;
    dark_config.algorithms = {ThemeAlgorithm::Dark};
    const auto dark = detail::derive_input_tokens(resolve_theme(dark_config));
    require(near(light.colors.filled_background.alpha(), .04F) &&
                near(light.colors.filled_hover_background.alpha(), .06F) &&
                near(dark.colors.filled_background.alpha(), .08F) &&
                near(dark.colors.filled_hover_background.alpha(), .12F),
            "Filled neutral palette differs from pinned Ant Design default/dark formulas");
}

void compact_and_family_forwarding() {
    for (const auto candidate :
         {InputVariant::Outlined, InputVariant::Borderless, InputVariant::Filled, InputVariant::Underlined}) {
        Fixture f;
        Signal<InputVariant> variant{candidate};
        f.inputs.mount(Content{[&] {
            SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[&] {
                             Input(InputProps{}.variant(variant).layout(LayoutStyle{}.width(dp(100))));
                             Button(ButtonProps{}, [] { Text(u8"B"); });
                         }});
            Password(PasswordProps{}.variant(variant).visibilityToggle(false));
            Search(SearchProps{}.variant(variant));
        }});
        f.synchronize();
        const auto input = f.inputs.mounted_inputs().front();
        for (const auto mounted : f.inputs.mounted_inputs()) {
            require(f.inputs.variant(mounted.component) == candidate, "Input family lost variant forwarding");
        }
        const auto button = f.buttons.mounted_buttons().front();
        const auto input_bounds = f.nodes.require(input.node).bounds;
        const auto button_bounds = f.nodes.require(button.node).bounds;
        const auto overlap = candidate == InputVariant::Outlined || candidate == InputVariant::Filled ? 1.0F : 0.0F;
        require(near(input_bounds.x + input_bounds.width - button_bounds.x, overlap),
                "variant Compact overlap incorrect");
        const auto root = f.buttons.components().root_components().front();
        const auto group = f.buttons.components().state<detail::SpaceCompactState>(root)->context;
        require((group->seams().empty()) == (overlap == 0), "borderless Compact invented a seam");
        require(f.buttons.snapshot(f.buttons.mounted_buttons().back().component).variant ==
                    (candidate == InputVariant::Outlined ? ButtonVariant::Outlined : ButtonVariant::Text),
                "Search action variant was not forwarded");
    }
}
} // namespace

int main() {
    try {
        variant_matrix();
        retained_switches_and_tokens();
        compact_and_family_forwarding();
        std::cout << "Native Input variants, tokens, retained state and Compact passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
