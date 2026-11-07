#include "component/selection_component.hpp"
#include "support/config_provider_consumers.hpp"

#include <array>
#include <string_view>

void config_provider_form_sources();
void config_provider_compound_consumers();
void config_provider_popup_and_service();
void config_provider_collections_and_idle();
void config_provider_getter_retirement();
void config_provider_boundary_and_button_rtl();
void config_provider_observer_retirement();
void config_provider_generated_option_sources();

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n_components;
using ryn_test::config_consumers::ConsumerFixture;

void sources_and_windows() {
    ConsumerFixture fixture;
    ConsumerFixture other;
    Signal<ControlSize> size{ControlSize::Large};
    Signal<bool> disabled{true};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    int mounts{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.size(size).disabled(disabled).direction(direction), ConfigProviderContent{[&] {
                ++mounts;
                Button(ButtonProps{}, [] { Text(u8"Inherited"); });
                Button(ButtonProps{}.size(ControlSize::Middle).disabled(false), [] { Text(u8"Explicit"); });
                Input(InputProps{});
                Input(InputProps{}.size(ControlSize::Middle).disabled(false).direction(TextDirection::Auto));
                Switch(SwitchProps{});
                ConfigProvider(ConfigProviderProps{}.disabled(false).size(ControlSize::Small),
                               ConfigProviderContent{[] { Button(ButtonProps{}, [] { Text(u8"Nested"); }); }});
            }});
    }});
    other.services.mount(Content{[] {
        Input(InputProps{});
        Button(ButtonProps{}, [] { Text(u8"Other window"); });
    }});
    synchronize(fixture);
    synchronize(other);
    const auto inherited = fixture.buttons.mounted_buttons()[0];
    const auto pinned = fixture.buttons.mounted_buttons()[1];
    const auto nested = fixture.buttons.mounted_buttons()[2];
    const auto input = fixture.inputs.mounted_inputs()[0];
    const auto explicit_input = fixture.inputs.mounted_inputs()[1];
    const auto sw = fixture.selections.mounted()[0];
    require(fixture.buttons.snapshot(inherited.component).size == ControlSize::Large &&
                fixture.buttons.snapshot(inherited.component).disabled &&
                fixture.buttons.snapshot(pinned.component).size == ControlSize::Middle &&
                !fixture.buttons.snapshot(pinned.component).disabled &&
                fixture.buttons.snapshot(nested.component).size == ControlSize::Small &&
                !fixture.buttons.snapshot(nested.component).disabled,
            "Button ignored Provider or explicit Middle/false/nested sources");
    require(
        fixture.inputs.size(input.component) == ControlSize::Large &&
            fixture.inputs.size(explicit_input.component) == ControlSize::Middle &&
            std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(input.component)).direction() ==
                TextDirection::RightToLeft &&
            std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(explicit_input.component)).direction() ==
                TextDirection::Auto &&
            fixture.selections.snapshot(sw.component).disabled,
        "Input/Selection did not consume native defaults");
    size.set(ControlSize::Small);
    disabled.set(false);
    direction.set(TextDirection::LeftToRight);
    synchronize(fixture);
    require(
        fixture.inputs.mounted_inputs()[0].editor == input.editor && mounts == 1 &&
            fixture.buttons.snapshot(inherited.component).size == ControlSize::Small &&
            fixture.buttons.snapshot(pinned.component).size == ControlSize::Middle &&
            fixture.inputs.size(explicit_input.component) == ControlSize::Middle &&
            std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(explicit_input.component)).direction() ==
                TextDirection::Auto,
        "Provider changed explicit provenance or remounted its slot");
    require(other.inputs.size(other.inputs.mounted_inputs()[0].component) == ControlSize::Middle &&
                std::as_const(other.scene)
                        .text_state(other.inputs.text_scene(other.inputs.mounted_inputs()[0].component))
                        .direction() == TextDirection::Auto &&
                !other.buttons.snapshot(other.buttons.mounted_buttons()[0].component).disabled,
            "Provider state crossed windows or changed the no-Provider default");
}

void compact_and_group_authority() {
    ConsumerFixture fixture;
    Signal<ControlSize> size{ControlSize::Large};
    Signal<ControlSize> compact_size{ControlSize::Small};
    Signal<bool> disabled{true};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.size(size).disabled(disabled), ConfigProviderContent{[&] {
                           SpaceCompact(SpaceCompactProps{}.size(compact_size), SpaceCompactContent{[] {
                                            Button(ButtonProps{}, [] { Text(u8"Compact"); });
                                            Button(ButtonProps{}.size(ControlSize::Middle), [] { Text(u8"Explicit"); });
                                            Input(InputProps{}.disabled(false));
                                        }});
                           CheckboxGroup(CheckboxGroupProps{}.disabled(true), CheckboxGroupContent{[] {
                                             Checkbox(CheckboxProps{}.value(u8"a").disabled(false));
                                         }});
                           RadioGroup(RadioGroupProps{}.disabled(true),
                                      RadioGroupContent{[] { Radio(RadioProps{}.value(u8"a").disabled(false)); }});
                       }});
    }});
    synchronize(fixture);
    const auto buttons = fixture.buttons.mounted_buttons();
    require(fixture.buttons.snapshot(buttons[0].component).size == ControlSize::Small &&
                fixture.buttons.snapshot(buttons[1].component).size == ControlSize::Middle &&
                fixture.inputs.size(fixture.inputs.mounted_inputs()[0].component) == ControlSize::Small,
            "Compact or explicit control lost its size priority");
    disabled.set(false);
    size.set(ControlSize::Middle);
    compact_size.set(ControlSize::Large);
    synchronize(fixture);
    require(fixture.buttons.snapshot(buttons[0].component).size == ControlSize::Large &&
                fixture.buttons.snapshot(buttons[1].component).size == ControlSize::Middle,
            "dynamic Compact lost priority over Provider");
    for (const auto& member : fixture.selections.mounted()) {
        require(fixture.selections.snapshot(member.component).disabled,
                "explicit false or Provider update released Group authority");
    }
}

void dirty_editor_and_real_input_gates() {
    ConsumerFixture fixture;
    Signal<bool> disabled{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    InputRef reference;
    int changes{};
    int clicks{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.disabled(disabled).size(size).direction(direction), ConfigProviderContent{[&] {
                Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                         Input(
                             InputProps{}.defaultValue(u8"abcdef").ref(reference).onChange([&](String) { ++changes; }));
                         Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Action"); });
                         Checkbox(CheckboxProps{}, CheckboxLabel{[] { Text(u8"Choice"); }});
                     }});
            }});
    }});
    synchronize(fixture);
    const auto input = fixture.inputs.mounted_inputs()[0];
    require(reference.focus() && reference.select(5, 1), "real editor selection failed");
    auto& editor = fixture.inputs.editors().require(input.editor);
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto session = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, session})), "real preedit failed");
    size.set(ControlSize::Large);
    direction.set(TextDirection::RightToLeft);
    synchronize(fixture);
    require(editor.value() == "abcdef" && editor.selection() == selection && editor.composition().active &&
                editor.history().undo_count == history.undo_count && fixture.inputs.sessions().active() == session &&
                changes == 0,
            "presentation defaults reset draft/selection/composition/undo/session");
    disabled.set(true);
    synchronize(fixture);
    require(!reference.focus() && !fixture.inputs.sessions().active().valid() && !editor.composition().active &&
                editor.value() == "abcdef" && editor.selection() == selection && changes == 0,
            "disabled did not retire actual IME/focus eligibility or reset business state");
    disabled.set(false);
    synchronize(fixture);
    require(reference.focus() && fixture.inputs.mounted_inputs()[0].editor == input.editor &&
                editor.selection() == selection,
            "reenabling replaced editor or lost dirty selection");
    const auto button = fixture.buttons.mounted_buttons()[0];
    const auto node = fixture.nodes.require(button.node);
    const auto bounds = runtime::Rect{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                      node.bounds.width, node.bounds.height};
    const auto mouse = PointerIdentity::mouse();
    fixture.services.pointer().dispatch({mouse, PointerAction::down, PointerButton::primary,
                                         bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    require(fixture.services.pointer().state(mouse)->capture == button.interaction,
            "real Button press did not capture");
    disabled.set(true);
    require(!fixture.services.pointer().state(mouse)->capture &&
                !fixture.buttons.snapshot(button.component).pointer_pressed,
            "Provider disable retained actual pointer capture");
    fixture.services.pointer().dispatch(
        {mouse, PointerAction::up, PointerButton::primary, bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    fixture.services.focus().dispatch({Key::enter, KeyAction::down});
    fixture.services.focus().dispatch({Key::enter, KeyAction::up});
    require(clicks == 0 && changes == 0, "disabled pointer/key activated a consumer");
    disabled.set(false);
    synchronize(fixture);
    const auto checkbox = fixture.selections.mounted().front();
    const auto& check_node = fixture.nodes.require(checkbox.node);
    const auto x = check_node.bounds.x + check_node.translation.x + check_node.bounds.width / 2;
    const auto y = check_node.bounds.y + check_node.translation.y + check_node.bounds.height / 2;
    fixture.services.pointer().dispatch({mouse, PointerAction::down, PointerButton::primary, x, y});
    require(fixture.services.pointer().state(mouse)->capture == checkbox.interaction,
            "reenabled Checkbox retained its old ineligible hit snapshot");
    disabled.set(true);
    require(!fixture.services.pointer().state(mouse)->capture &&
                !fixture.selections.snapshot(checkbox.component).pointer_pressed,
            "Provider disable retained actual Checkbox capture");
    fixture.services.pointer().dispatch({mouse, PointerAction::up, PointerButton::primary, x, y});
    require(!fixture.selections.snapshot(checkbox.component).checked, "cancelled Checkbox gesture changed its value");
}

void blur_can_retire_owner() {
    ConsumerFixture fixture;
    Signal<bool> disabled{false};
    InputRef reference;
    const auto owner = mount_owned(
        fixture, Content{[&] {
            ConfigProvider(ConfigProviderProps{}.disabled(disabled), ConfigProviderContent{[&] {
                               Input(InputProps{}.ref(reference).onBlur([&] { fixture.services.dispose(); }));
                           }});
        }});
    synchronize(fixture);
    require(reference.focus(), "retirement focus failed");
    disabled.set(true);
    require(!fixture.services.components().contains(owner) && fixture.inputs.mounted_inputs().empty(),
            "Provider disable continued through disposed editor owner");
}
} // namespace

int main(int argc, char** argv) {
    ryn_test::i18n::suppress_dialogs();
    const std::array cases{
        std::pair{"sources", sources_and_windows},
        std::pair{"compact_group", compact_and_group_authority},
        std::pair{"input_gates", dirty_editor_and_real_input_gates},
        std::pair{"reentry", blur_can_retire_owner},
        std::pair{"form_sources", config_provider_form_sources},
        std::pair{"compound", config_provider_compound_consumers},
        std::pair{"popup_service", config_provider_popup_and_service},
        std::pair{"collections_idle", config_provider_collections_and_idle},
        std::pair{"getter_retirement", config_provider_getter_retirement},
        std::pair{"boundary_rtl", config_provider_boundary_and_button_rtl},
        std::pair{"observer_retirement", config_provider_observer_retirement},
        std::pair{"generated_options", config_provider_generated_option_sources},
    };
    try {
        for (const auto& [name, run] : cases) {
            if (argc == 1 || std::string_view{argv[1]} == name) {
                run();
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
