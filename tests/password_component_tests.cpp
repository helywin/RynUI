#include "support/input_fixture.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }

InteractionId toggle(Fixture& f, InteractionId input) {
    for(const auto id : f.buttons.interactions().declaration_order()) {
        const auto* record = f.buttons.interactions().find(id);
        if(record && record->parent == input && id != input) return id;
    }
    throw std::runtime_error("Password toggle interaction absent");
}
void click(Fixture& f, InteractionId id) {
    const auto bounds = f.nodes.require(f.buttons.interactions().require(id).node).bounds;
    const auto x = bounds.x + bounds.width / 2, y = bounds.y + bounds.height / 2;
    f.buttons.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y, 1});
    f.buttons.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y, 1});
    f.synchronize();
}
void send(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none, KeyAction action = KeyAction::down) {
    f.buttons.focus().dispatch({key, action, modifiers, false, KeyModifier::control});
}

void default_and_composition() {
    Fixture f;
    f.inputs.mount(Content{[] {
        Password(PasswordProps{}.defaultValue(u8"密🙂"));
        Input(InputProps{}.defaultValue(u8"sibling"));
    }});
    require(f.inputs.mounted_inputs().size() == 2, "Password duplicated Input host");
    const auto password = f.inputs.mounted_inputs()[0], sibling = f.inputs.mounted_inputs()[1];
    f.synchronize();
    const auto toggler = toggle(f, password.interaction);
    require(!f.buttons.interactions().require(toggler).focus_on_pointer, "toggle pointer focus policy absent");
    require(f.inputs.display_snapshot(password.component).text == String{u8"••"}.bytes()
        && f.scene.text_state(f.inputs.text_scene(password.component)).content().bytes() == String{u8"••"}.bytes(),
        "hidden Password scene leaked original value");
    require(f.buttons.focus().request_focus(password.interaction, FocusModality::keyboard), "Password focus failed");
    require(f.platform.last_properties.type == TextInputType::password_hidden && !f.platform.last_properties.autocorrect,
        "hidden Password platform properties incorrect");
    auto& editor = f.inputs.editors().require(password.editor);
    require(bool(editor.move(TextCaretMove::end)), "Password caret move failed");
    const auto before = editor.selection();
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})), "Password preedit failed");
    const auto cancels = f.platform.cancels;
    click(f, toggler);
    require(f.buttons.focus().state().focused == password.interaction && editor.selection() == before
        && editor.composition().active && f.inputs.sessions().active() == stamp
        && f.platform.cancels == cancels,
        "pointer reveal stole focus, caret or composition");
    require(f.inputs.display_snapshot(password.component).text == String{u8"密🙂ni"}.bytes(),
        "Password reveal did not show logical preedit");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"好"}, stamp})), "Password commit failed");
    require(editor.value() == String{u8"密🙂好"}.bytes()
        && f.platform.last_properties.type == TextInputType::password_visible,
        "Password visible platform type did not apply after composition");
    require(f.buttons.focus().request_focus(toggler, FocusModality::keyboard), "toggle keyboard focus failed");
    send(f, Key::space); send(f, Key::space, KeyModifier::none, KeyAction::up);
    require(f.inputs.display_snapshot(password.component).text == String{u8"•••"}.bytes(),
        "keyboard toggle did not mask Password");
    require(f.buttons.destroy(password.component) && f.inputs.editors().size() == 1
        && !f.buttons.interactions().contains(toggler), "Password destroy retained toggle/editor");
    require(f.buttons.focus().request_focus(sibling.interaction, FocusModality::keyboard),
        "sibling Input failed after Password destroy");
}

void controlled_and_clipboard() {
    Fixture f;
    Signal<bool> visible{false}, disabled{false};
    int requests{};
    f.inputs.mount(Content{[&] {
        Password(PasswordProps{}.defaultValue(u8"secret").visible(visible).disabled(disabled)
            .onVisibleChange([&](bool next) { require(next, "wrong visibility request"); ++requests; }));
    }});
    const auto password = f.inputs.mounted_inputs().front();
    f.synchronize();
    const auto toggler = toggle(f, password.interaction);
    require(f.buttons.focus().request_focus(password.interaction, FocusModality::keyboard), "controlled Password focus failed");
    auto& editor = f.inputs.editors().require(password.editor);
    send(f, Key::a, KeyModifier::control);
    send(f, Key::c, KeyModifier::control); send(f, Key::x, KeyModifier::control);
    require(f.platform.writes == 0 && editor.value() == "secret", "hidden copy/cut leaked Password");
    f.platform.clipboard = String{u8"x"};
    send(f, Key::v, KeyModifier::control);
    require(f.platform.reads == 1 && editor.value() == "x", "hidden paste failed");
    click(f, toggler);
    require(requests == 1 && f.inputs.display_snapshot(password.component).text == String{u8"•"}.bytes(),
        "controlled Password changed before visible echo");
    visible.set(true);
    require(f.inputs.display_snapshot(password.component).text == "x", "controlled visibility echo failed");
    disabled.set(true);
    require(!f.buttons.interactions().require(toggler).eligible, "disabled toggle remained eligible");
    click(f, toggler);
    require(requests == 1, "disabled Password toggled");
}

void invalid_and_no_toggle() {
    Fixture invalid;
    bool rejected{};
    try { invalid.inputs.mount(Content{[] { Password(PasswordProps{}.value(u8"a").defaultValue(u8"b")); }}); }
    catch(const std::invalid_argument&) { rejected = true; }
    require(rejected && invalid.inputs.editors().size() == 0 && invalid.buttons.interactions().size() == 0,
        "invalid Password acquired resources");
    Fixture f;
    Signal<bool> visible{false};
    f.inputs.mount(Content{[&] { Password(PasswordProps{}.defaultValue(u8"abc")
        .visible(visible).visibilityToggle(false)); }});
    const auto input = f.inputs.mounted_inputs().front();
    f.synchronize();
    require(f.buttons.interactions().size() == 1
        && f.inputs.display_snapshot(input.component).text == String{u8"•••"}.bytes(),
        "disabled visibility toggle mounted interaction");
    visible.set(true);
    require(f.inputs.display_snapshot(input.component).text == "abc",
        "toggle-free controlled visibility lost its subscription");
}
}

int main() {
    try { default_and_composition(); controlled_and_clipboard(); invalid_and_no_toggle();
        std::cout << "Password component and editing passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
