#include "support/input_fixture.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}

InteractionId clear_action(Fixture& fixture, InteractionId input) {
    for(const auto id : fixture.buttons.interactions().declaration_order()) {
        const auto* record = fixture.buttons.interactions().find(id);
        if(record && record->parent == input && id != input) return id;
    }
    throw std::runtime_error("Input clear action absent");
}

void click(Fixture& fixture, InteractionId action) {
    const auto bounds = fixture.nodes.require(fixture.buttons.interactions().require(action).node).bounds;
    const auto x = bounds.x + bounds.width / 2, y = bounds.y + bounds.height / 2;
    fixture.buttons.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down,
        PointerButton::primary, x, y, 1});
    fixture.buttons.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up,
        PointerButton::primary, x, y, 1});
    fixture.synchronize();
}

void clear_uncontrolled_and_composition() {
    Fixture fixture;
    int changes{}, suffix_runs{};
    String last;
    fixture.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"初始值").allowClear(true)
            .onChange([&](String value) { ++changes; last = std::move(value); }),
            {}, InputSuffix{[&] { ++suffix_runs; Text(u8"后"); }});
    }});
    const auto input = fixture.inputs.mounted_inputs().front();
    fixture.synchronize();
    const auto action = clear_action(fixture, input.interaction);
    require(suffix_runs == 1 && fixture.buttons.interactions().require(action).eligible,
        "clear action or custom suffix missing");
    require(!fixture.buttons.interactions().require(action).focus_on_pointer,
        "clear pointer focus policy incorrect");
    require(fixture.buttons.focus().request_focus(input.interaction, FocusModality::keyboard),
        "Input focus failed");
    const auto stamp = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
        "composition start failed");
    const auto cancels = fixture.platform.cancels;
    click(fixture, action);
    require(fixture.inputs.editors().require(input.editor).value().empty() && changes == 1
        && last.empty() && fixture.platform.cancels > cancels,
        "clear failed to cancel composition and emit one empty change");
    require(fixture.buttons.focus().state().focused == input.interaction
        && fixture.inputs.sessions().active() == stamp,
        "clear stole Input focus or session");
    require(!fixture.buttons.interactions().require(action).eligible
        && fixture.nodes.require(fixture.buttons.interactions().require(action).node).bounds.width == 0,
        "empty clear action retained hit area");
    require(suffix_runs == 1, "clear remounted custom suffix");
    require(fixture.buttons.destroy(input.component)
        && !fixture.buttons.interactions().contains(action)
        && fixture.inputs.editors().size() == 0,
        "clear action or editor leaked after destroy");
}

void controlled_reactive_and_keyboard() {
    Fixture fixture;
    Signal<String> value{String{u8"abc"}};
    Signal<bool> allowed{true}, disabled{false}, read_only{false};
    int changes{};
    fixture.inputs.mount(Content{[&] {
        Input(InputProps{}.value(value).allowClear(allowed).disabled(disabled)
            .readOnly(read_only).onChange([&](String next) {
                require(next.empty(), "clear callback did not receive empty value");
                ++changes;
            }));
    }});
    const auto input = fixture.inputs.mounted_inputs().front();
    fixture.synchronize();
    const auto action = clear_action(fixture, input.interaction);
    require(fixture.buttons.interactions().require(action).eligible, "controlled clear missing");
    allowed.set(false); fixture.synchronize();
    require(!fixture.buttons.interactions().require(action).eligible, "allowClear=false still eligible");
    allowed.set(true); disabled.set(true); fixture.synchronize();
    require(!fixture.buttons.interactions().require(action).eligible, "disabled clear still eligible");
    disabled.set(false); read_only.set(true); fixture.synchronize();
    require(!fixture.buttons.interactions().require(action).eligible, "read-only clear still eligible");
    read_only.set(false); fixture.synchronize();
    require(fixture.buttons.focus().request_focus(action, FocusModality::keyboard),
        "clear action keyboard focus failed");
    fixture.buttons.focus().dispatch({Key::space, KeyAction::down,
        KeyModifier::none, false, KeyModifier::control});
    fixture.buttons.focus().dispatch({Key::space, KeyAction::up,
        KeyModifier::none, false, KeyModifier::control});
    fixture.synchronize();
    require(changes == 1 && fixture.inputs.editors().require(input.editor).value().empty(),
        "keyboard clear failed");
    value.set(String{u8"external"}); fixture.synchronize();
    require(fixture.inputs.editors().require(input.editor).value() == "external"
        && fixture.buttons.interactions().require(action).eligible && changes == 1,
        "controlled echo did not restore visible value");
}

void callback_may_destroy_input() {
    Fixture fixture;
    runtime::ComponentId component;
    int callbacks{};
    fixture.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"erase").allowClear(true)
            .onChange([&](String next) {
                require(next.empty(), "self-destroy clear value incorrect");
                ++callbacks;
                require(fixture.buttons.destroy(component), "callback could not destroy Input");
            }));
    }});
    const auto input = fixture.inputs.mounted_inputs().front();
    component = input.component;
    fixture.synchronize();
    click(fixture, clear_action(fixture, input.interaction));
    require(callbacks == 1 && fixture.inputs.editors().size() == 0
        && fixture.buttons.interactions().size() == 0,
        "clear callback self-destroy retained resources");
}
} // namespace

int main() {
    try {
        clear_uncontrolled_and_composition();
        controlled_reactive_and_keyboard();
        callback_may_destroy_input();
        std::cout << "Input clear action passed\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
