#include "support/input_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <iostream>

namespace {
using namespace ryn; using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void send(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none, bool repeat = false,
    KeyAction action = KeyAction::down, KeyModifier primary = KeyModifier::control) {
    f.buttons.focus().dispatch({key, action, modifiers, repeat, primary});
}
void navigation_and_repeat() {
    Fixture f; int changes{}, submits{};
    f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"a中👩‍👩‍👧‍👦b")
        .onChange([&](String) { ++changes; }).onSubmit([&](String) { ++submits; })); }});
    const auto input = f.inputs.mounted_inputs().front(); f.synchronize();
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::pointer), "focus failed");
    auto& editor = f.inputs.editors().require(input.editor);
    send(f, Key::home); send(f, Key::right); send(f, Key::right, KeyModifier::shift, true);
    require(editor.selection() == TextSelection{1, 4}, "Shift/repeat split CJK");
    send(f, Key::right, KeyModifier::shift);
    require(editor.selection() == TextSelection{1, editor.value().size() - 1}, "Shift split emoji");
    send(f, Key::left);
    require(editor.selection() == TextSelection{1, 1}, "Left did not collapse backward");
    send(f, Key::end, KeyModifier::shift); send(f, Key::home, KeyModifier::shift);
    require(editor.selection() == TextSelection{1, 0}, "Home/End reversed anchor");
    send(f, Key::end); send(f, Key::backspace, KeyModifier::none, true);
    require(editor.value() == String{u8"a中👩‍👩‍👧‍👦"}.bytes(), "repeat Backspace failed");
    send(f, Key::backspace);
    require(editor.value() == String{u8"a中"}.bytes(), "Backspace split emoji");
    send(f, Key::home); send(f, Key::delete_forward, KeyModifier::none, true);
    require(editor.value() == String{u8"中"}.bytes() && changes == 3, "Delete change callback mismatch");
    send(f, Key::a); send(f, Key::space);
    require(editor.value() == String{u8"中"}.bytes(), "key event inserted characters");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"x"}, f.inputs.sessions().active()})), "text commit failed");
    require(changes == 4, "text commit duplicated change");
    send(f, Key::enter); send(f, Key::enter, KeyModifier::none, true); send(f, Key::enter, KeyModifier::none, false, KeyAction::up);
    require(submits == 1 && f.buttons.focus().diagnostics().activations == 0, "Enter repeat/activation duplicated submit");
    require(f.buttons.focus().state().focus_visible, "editing did not establish keyboard modality");
}
void shortcuts() {
    for(const auto primary : {KeyModifier::control, KeyModifier::meta}) {
        Fixture f; Signal<String> value{String{u8"hello"}}; int changes{};
        f.inputs.mount(Content{[&] { Input(InputProps{}.value(value).onChange([&](String next) {
            ++changes; value.set(std::move(next)); })); }});
        const auto input = f.inputs.mounted_inputs().front(); f.synchronize();
        require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "focus failed");
        auto& editor = f.inputs.editors().require(input.editor);
        const auto key = [&](Key k, bool repeat = false, bool shift = false) {
            send(f, k, shift ? primary | KeyModifier::shift : primary, repeat, KeyAction::down, primary);
        };
        key(Key::a); key(Key::c);
        require(f.platform.clipboard == String{u8"hello"} && f.platform.writes == 1, "primary copy failed");
        key(Key::c, true); key(Key::x, true);
        require(f.platform.writes == 1 && editor.value() == "hello", "repeat shortcut executed");
        key(Key::x);
        require(editor.value().empty() && changes == 1, "primary cut failed");
        key(Key::z); require(editor.value() == "hello", "undo failed");
        key(Key::y); require(editor.value().empty(), "redo Y failed");
        key(Key::z); key(Key::z, false, true);
        require(editor.value().empty(), "Shift Z redo failed");
        f.platform.clipboard = String{u8"中\r\n文"}; key(Key::v);
        require(editor.value() == String{u8"中文"}.bytes() && changes == 6, "paste normalization/controlled echo failed");
        key(Key::v, true); require(f.platform.reads == 1, "repeat paste read clipboard");
        const auto before = editor.selection();
        send(f, Key::a, KeyModifier::control | KeyModifier::alt, false, KeyAction::down, primary);
        require(editor.selection() == before, "AltGr was treated as primary shortcut");
        send(f, Key::a, primary == KeyModifier::control ? KeyModifier::meta : KeyModifier::control, false, KeyAction::down, primary);
        require(editor.selection() == before, "foreign primary modifier selected all");
        f.platform.clipboard_failure = true; key(Key::a); key(Key::x); key(Key::v);
        require(editor.value() == String{u8"中文"}.bytes(), "clipboard failure mutated text");
    }
}
void composition_and_traversal() {
    Fixture f; int submits{};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"base").onSubmit([&](String) { ++submits; })); Input(InputProps{});
    }});
    const auto first = f.inputs.mounted_inputs()[0], second = f.inputs.mounted_inputs()[1];
    f.synchronize(); require(f.buttons.focus().request_focus(first.interaction, FocusModality::keyboard), "focus failed");
    auto& editor = f.inputs.editors().require(first.editor);
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼音"}, {2, 0}, f.inputs.sessions().active()})), "preedit failed");
    const auto selection = editor.selection();
    for(auto key : {Key::left, Key::right, Key::home, Key::end, Key::backspace, Key::delete_forward, Key::enter, Key::tab}) send(f, key);
    for(auto key : {Key::a, Key::c, Key::x, Key::v, Key::z, Key::y}) send(f, key, KeyModifier::control);
    require(editor.composition().active && editor.value() == "base" && editor.selection() == selection
        && f.buttons.focus().state().focused == first.interaction && submits == 0 && f.platform.reads == 0
        && f.platform.writes == 0, "IME key priority lost");
    send(f, Key::escape);
    require(!editor.composition().active && editor.value() == "base", "Escape failed to cancel composition");
    send(f, Key::tab); require(f.buttons.focus().state().focused == second.interaction, "unconsumed Tab did not traverse");
    send(f, Key::tab, KeyModifier::shift); require(f.buttons.focus().state().focused == first.interaction, "Shift Tab failed");
}
void read_only_and_destroy() {
    Fixture f; Signal<bool> disabled{false}, read_only{true}; int changes{}, submits{};
    f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"abc").disabled(disabled).readOnly(read_only)
        .onChange([&](String) { ++changes; }).onSubmit([&](String) { ++submits; })); }});
    const auto input = f.inputs.mounted_inputs().front(); f.synchronize();
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "readOnly focus failed");
    auto& editor = f.inputs.editors().require(input.editor);
    send(f, Key::a, KeyModifier::control); send(f, Key::c, KeyModifier::control);
    for(auto key : {Key::x, Key::v, Key::z, Key::y}) send(f, key, KeyModifier::control);
    send(f, Key::backspace); send(f, Key::delete_forward); send(f, Key::enter);
    require(editor.value() == "abc" && changes == 0 && submits == 0 && f.platform.writes == 1 && f.platform.reads == 0,
        "readOnly keyboard eligibility failed");
    disabled.set(true); send(f, Key::home);
    require(editor.selection() == TextSelection{0, 3} && !f.buttons.focus().state().focused, "disabled handled key");
    for(int mode = 0; mode < 3; ++mode) {
        Fixture dying; runtime::ComponentId id;
        const auto destroy = [&] { require(dying.buttons.destroy(id), "callback destroy failed"); };
        dying.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"abc")
            .onChange([&](String) { if(mode == 0) destroy(); }).onSubmit([&](String) { if(mode == 1) destroy(); })); }});
        const auto mounted = dying.inputs.mounted_inputs().front(); id = mounted.component;
        require(dying.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "destroy focus failed");
        if(mode == 2) { dying.platform.clipboard = String{u8"x"}; dying.platform.on_clipboard = destroy; }
        send(dying, mode == 0 ? Key::delete_forward : mode == 1 ? Key::enter : Key::v,
            mode == 2 ? KeyModifier::control : KeyModifier::none);
        require(dying.inputs.mounted_inputs().empty() && !dying.buttons.focus().state().focused
            && !dying.inputs.sessions().active().valid(), "keyboard callback retained owner");
    }
}
void allocation() {
    Fixture f; f.inputs.mount(Content{[] { Input(InputProps{}.defaultValue(u8"abc中文")); }});
    const auto input = f.inputs.mounted_inputs().front(); f.synchronize();
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "allocation focus failed");
    const auto cycle = [&](int i) { send(f, i % 2 ? Key::home : Key::end, KeyModifier::shift, true); f.synchronize(); };
    for(int i = 0; i < 20; ++i) cycle(i);
    ryn_test::allocation::begin(); for(int i = 0; i < 20000; ++i) cycle(i);
    const auto count = ryn_test::allocation::end(); require(count == 0, "keyboard selection allocated after warmup");
}
void callback_eligibility() {
    for(bool make_disabled : {false, true}) {
        Fixture f; Signal<bool> disabled{false}, read_only{false};
        f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"abc").disabled(disabled).readOnly(read_only)
            .onChange([&](String) { if(make_disabled) disabled.set(true); else read_only.set(true); })); }});
        const auto input = f.inputs.mounted_inputs().front();
        require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "callback focus failed");
        send(f, Key::delete_forward);
        require(!f.inputs.sessions().active().valid(), "callback eligibility retained session");
        require(f.buttons.focus().state().focused.has_value() != make_disabled, "callback eligibility focus mismatch");
        disabled.set(false); read_only.set(false);
        require(f.inputs.sessions().active().valid() != make_disabled, "unfocused callback reenabled native session");
        require(f.buttons.focus().diagnostics().reentrant_rejections == 0, "eligibility update attempted nested focus dispatch");
    }
}
}
int main() {
    try { navigation_and_repeat(); shortcuts(); composition_and_traversal(); read_only_and_destroy(); allocation(); callback_eligibility(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
