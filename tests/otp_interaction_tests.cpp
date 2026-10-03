#include "support/input_fixture.hpp"
#include "component/otp_component.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void key(Fixture& f, input::Key key, input::KeyModifier modifiers = input::KeyModifier::none) {
    f.services.focus().dispatch({key, input::KeyAction::down, modifiers});
    f.services.focus().dispatch({key, input::KeyAction::up, modifiers});
}

detail::MountedInputComponent cell(Fixture& f, runtime::ComponentId id) {
    for (const auto& mounted : f.inputs.mounted_inputs()) {
        if (mounted.component == id) {
            return mounted;
        }
    }
    throw std::runtime_error("missing retained cell");
}

void click(Fixture& f, const detail::MountedInputComponent& mounted) {
    const auto& node = f.nodes.require(mounted.node);
    const auto x = node.bounds.x + node.translation.x + node.bounds.width / 2;
    const auto y = node.bounds.y + node.translation.y + node.bounds.height / 2;
    const auto pointer = input::PointerIdentity::mouse();
    f.services.pointer().dispatch({pointer, input::PointerAction::down, input::PointerButton::primary, x, y});
    f.services.pointer().dispatch({pointer, input::PointerAction::up, input::PointerButton::primary, x, y});
}

void commit(Fixture& f, const String& text) {
    check(bool(f.inputs.dispatch(input::TextCommitted{text, f.inputs.sessions().active()})), "OTP commit rejected");
}

void navigation_ime_and_retirement() {
    Fixture f;
    Signal<std::size_t> length{4};
    Signal<OTPDirection> direction{OTPDirection::LeftToRight};
    Signal<OTPMask> mask{OTPMask{}};
    OTPRef ref;
    std::vector<std::size_t> focused;
    std::vector<std::size_t> blurred;
    int edits{};
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}
                .length(length)
                .direction(direction)
                .mask(mask)
                .defaultValue(u8"ab")
                .autoFocus()
                .ref(ref)
                .onFocus([&](std::size_t index) { focused.push_back(index); })
                .onBlur([&](std::size_t index) { blurred.push_back(index); })
                .onInput([&](const auto&) { ++edits; }));
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"after"); }});
    }});
    f.synchronize();
    const auto group = f.services.otp().mounted().front().component;
    const auto ids = f.services.otp().cell_components(group);
    const auto first = cell(f, ids[0]);
    const auto second = cell(f, ids[1]);
    const auto third = cell(f, ids[2]);
    const auto fourth = cell(f, ids[3]);
    check(f.inputs.sessions().active().owner == first.editor &&
              f.inputs.editors().require(first.editor).selection() == input::TextSelection{0, 1},
          "mount autofocus did not select first cell");
    click(f, fourth);
    check(f.inputs.sessions().active().owner == third.editor && focused == std::vector<std::size_t>{0, 3, 2} &&
              blurred == std::vector<std::size_t>{0, 3},
          "first-hole focus callbacks were not actual indexed transitions");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {}, stamp})), "OTP preedit rejected");
    key(f, input::Key::left);
    key(f, input::Key::enter);
    key(f, input::Key::z, input::KeyModifier::control);
    check(f.inputs.sessions().active() == stamp && edits == 0 && f.services.otp().cells(group)[2].empty(),
          "OTP IME lost owner/navigation or published candidate");
    mask.set({true, String{u8"*"}});
    f.synchronize();
    check(f.inputs.sessions().active() == stamp && f.platform.last_properties.type == input::TextInputType::number &&
              f.inputs.display_snapshot(third.component).text == "**",
          "OTP mask did not defer native session hints during preedit");
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"中"}, stamp})), "OTP IME commit rejected");
    f.synchronize();
    check(edits == 1 && f.inputs.sessions().active().owner == fourth.editor &&
              f.platform.last_properties.type == input::TextInputType::password_hidden &&
              f.services.otp().cells(group)[2] == String{u8"中"},
          "OTP committed IME did not split/advance/refresh hints");
    check(!f.inputs.dispatch(input::TextCommitted{String{u8"bad"}, stamp}), "OTP stale IME stamp accepted");
    key(f, input::Key::backspace);
    check(f.inputs.sessions().active().owner == third.editor &&
              f.inputs.editors().require(third.editor).selection() == input::TextSelection{0, 3},
          "empty Backspace did not select previous grapheme");
    key(f, input::Key::left);
    check(f.inputs.sessions().active().owner == second.editor, "LTR left navigation");
    direction.set(OTPDirection::RightToLeft);
    key(f, input::Key::left);
    check(f.inputs.sessions().active().owner == third.editor, "RTL left navigation");
    key(f, input::Key::right);
    check(f.inputs.sessions().active().owner == second.editor, "RTL right navigation");
    key(f, input::Key::z, input::KeyModifier::control);
    key(f, input::Key::y, input::KeyModifier::control);
    check(edits == 1 && f.services.otp().cells(group)[2] == String{u8"中"}, "single cell undo corrupted group");
    key(f, input::Key::tab);
    check(f.inputs.sessions().active().owner == third.editor, "Tab did not follow normal declaration order");
    key(f, input::Key::tab);
    commit(f, String{u8"d"});
    key(f, input::Key::tab);
    check(!f.inputs.sessions().active().valid(), "Tab did not leave filled OTP");
    key(f, input::Key::tab, input::KeyModifier::shift);
    check(f.inputs.sessions().active().owner == fourth.editor, "Shift Tab did not return to final cell");
    const auto retiring = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"pre"}, {}, retiring})),
          "retirement preedit rejected");
    length.set(2);
    f.synchronize();
    check(f.inputs.sessions().active().owner == second.editor && f.inputs.editors().size() == 2 &&
              f.services.otp().cell_components(group) == std::vector<runtime::ComponentId>{ids[0], ids[1]} &&
              !f.inputs.dispatch(input::TextCommitted{String{u8"late"}, retiring}),
          "length did not retire active tail owner/stamp or transfer retained focus");
    check(f.inputs.synchronize_input_area(1, 320, 240) && f.platform.area.width > 0 && f.platform.area.height > 0,
          "OTP native input area absent");
    const auto copied = ref;
    bool wrong_thread = false;
    std::thread worker{[&] {
        try {
            static_cast<void>(copied.bound());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    }};
    worker.join();
    check(wrong_thread && copied.blur(), "OTPRef owner thread/copy binding failed");
    f.services.dispose();
    check(!copied.bound() && !ref.focus(), "OTP copied reference survived disposal");
}

void clipboard_and_eligibility() {
    Fixture f;
    Signal<bool> disabled{true};
    Signal<bool> read_only{true};
    Signal<OTPMask> mask{OTPMask{}};
    OTPRef ref;
    int edits{};
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}
                .length(4)
                .defaultValue(u8"abcd")
                .disabled(disabled)
                .readOnly(read_only)
                .mask(mask)
                .autoFocus()
                .ref(ref)
                .onInput([&](const auto&) { ++edits; }));
    }});
    f.synchronize();
    check(!f.inputs.sessions().active().valid() && !ref.focus(), "disabled OTP autofocus/ref accepted");
    disabled.set(false);
    f.synchronize();
    check(!f.inputs.sessions().active().valid(), "mount-only autofocus stole focus after enabling");
    check(ref.focus(), "readonly OTP did not focus");
    key(f, input::Key::c, input::KeyModifier::control);
    check(f.platform.clipboard == String{u8"a"} && f.platform.writes == 1, "readonly OTP copy failed");
    f.platform.clipboard = String{u8"1234"};
    key(f, input::Key::x, input::KeyModifier::control);
    key(f, input::Key::v, input::KeyModifier::control);
    check(edits == 0 && f.platform.reads == 0 && f.platform.writes == 1, "readonly OTP cut/paste changed value");
    read_only.set(false);
    key(f, input::Key::v, input::KeyModifier::control);
    const auto group = f.services.otp().mounted().front().component;
    check(edits == 1 && f.services.otp().cells(group) ==
                            std::vector<String>{String{u8"1"}, String{u8"2"}, String{u8"3"}, String{u8"4"}},
          "clipboard paste did not fill OTP");
    mask.set({true, String{u8"•"}});
    f.synchronize();
    key(f, input::Key::c, input::KeyModifier::control);
    key(f, input::Key::x, input::KeyModifier::control);
    check(edits == 1 && f.platform.writes == 1, "masked OTP exported/cut raw value");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp})),
          "eligibility preedit failed");
    disabled.set(true);
    check(!f.inputs.sessions().active().valid() && !f.services.focus().state().focused && !ref.focus() &&
              !f.inputs.dispatch(input::TextCommitted{String{u8"late"}, stamp}),
          "disabled OTP retained session/focus");
}

void callback_and_formatter_reentry() {
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        Signal<String> value{String{u8"abc"}};
        Signal<std::size_t> length{4};
        OTPRef ref;
        runtime::ComponentId group;
        int partial{};
        int complete{};
        bool applying = false;
        f.inputs.mount(Content{[&] {
            OTP(OTPProps{}
                    .length(length)
                    .value(value)
                    .ref(ref)
                    .formatter([&](String candidate) {
                        if (applying && mode == 3) {
                            length.set(2);
                            value.set(String{u8"external"});
                        }
                        if (applying && mode == 4) {
                            f.services.destroy(group);
                        }
                        return candidate;
                    })
                    .onInput([&](const auto&) {
                        ++partial;
                        if (mode == 0) {
                            value.set(String{u8"0000"});
                        }
                        if (mode == 1) {
                            f.services.destroy(group);
                        }
                        if (mode == 2) {
                            value.set(String{u8"abcd"});
                        }
                    })
                    .onChange([&](String) { ++complete; }));
        }});
        f.synchronize();
        group = f.services.otp().mounted().front().component;
        const auto ids = f.services.otp().cell_components(group);
        f.services.focus().defer_focus(cell(f, ids[3]).interaction, input::FocusModality::keyboard);
        const auto stamp = f.inputs.sessions().active();
        applying = true;
        const auto result = f.inputs.dispatch(input::TextCommitted{String{u8"d"}, stamp});
        if (mode == 0) {
            check(result && partial == 1 && complete == 0 && value.get() == String{u8"0000"},
                  "conflicting partial callback did not cancel old completion");
        }
        if (mode == 1) {
            check(result && partial == 1 && complete == 0 && !ref.bound() && f.inputs.editors().size() == 0,
                  "partial self-unmount invoked old completion or leaked owners");
        }
        if (mode == 2) {
            check(result && partial == 1 && complete == 1 && value.get() == String{u8"abcd"},
                  "identical authoritative echo cancelled completion");
        }
        if (mode == 3) {
            check(!result && partial == 0 && complete == 0 &&
                      f.services.otp().cells(group) == std::vector<String>{String{u8"e"}, String{u8"x"}},
                  "formatter authoritative reentry published old transaction");
        }
        if (mode == 4) {
            check(!result && partial == 0 && complete == 0 && !ref.bound() && f.inputs.editors().size() == 0,
                  "formatter self-unmount leaked/reused owner");
        }
    }
    Fixture f;
    OTPRef ref;
    bool fail = false;
    int notifications{};
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}
                .length(4)
                .ref(ref)
                .defaultValue(u8"1234")
                .formatter([&](String value) {
                    if (fail) {
                        throw std::runtime_error("format failure");
                    }
                    return value;
                })
                .onInput([&](const auto&) { ++notifications; }));
    }});
    f.synchronize();
    const auto group = f.services.otp().mounted().front().component;
    check(ref.focus(), "throw formatter focus failed");
    const auto stamp = f.inputs.sessions().active();
    fail = true;
    check(!f.inputs.dispatch(input::TextCommitted{String{u8"bad"}, stamp}), "formatter exception not rejected");
    key(f, input::Key::right);
    key(f, input::Key::c, input::KeyModifier::control);
    check(notifications == 0 && f.services.otp().cells(group)[0] == String{u8"1"},
          "failed edit pending published from navigation");
    fail = false;
    commit(f, String{u8"x"});
    check(notifications == 1 && f.services.otp().cells(group)[1] == String{u8"x"}, "formatter did not recover");
}

void initial_formatter_and_explicit_focus() {
    Fixture f;
    Signal<String> value{String{u8"1234"}};
    Signal<std::size_t> length{4};
    InputRef outside;
    bool initial = true;
    bool transfer = false;
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}
                .value(value)
                .length(length)
                .formatter([&](String text) {
                    if (initial) {
                        initial = false;
                        length.set(3);
                        value.set(String{u8"98765"});
                    }
                    return text;
                })
                .onBlur([&](std::size_t index) {
                    if (transfer && index == 2) {
                        static_cast<void>(outside.focus());
                    }
                }));
        Input(InputProps{}.ref(outside));
    }});
    f.synchronize();
    const auto group = f.services.otp().mounted().front().component;
    const auto ids = f.services.otp().cell_components(group);
    check(ids.size() == 3 &&
              f.services.otp().cells(group) == std::vector<String>{String{u8"9"}, String{u8"8"}, String{u8"7"}},
          "initial formatter mutation was discarded by first controlled binding");
    f.services.focus().defer_focus(cell(f, ids[2]).interaction, input::FocusModality::keyboard);
    const auto outside_owner = f.inputs.mounted_inputs().back().editor;
    transfer = true;
    length.set(2);
    check(f.inputs.sessions().active().owner == outside_owner,
          "automatic shrink focus overrode explicit blur callback request");
    f.platform.clipboard = String{u8"x"};
    check(outside.blur(), "outside blur failed");
    f.services.focus().defer_focus(cell(f, ids[0]).interaction, input::FocusModality::keyboard);
    f.platform.on_clipboard = [&] {
        f.services.destroy(group);
    };
    key(f, input::Key::v, input::KeyModifier::control);
    check(f.services.otp().mounted().empty() && f.inputs.editors().size() == 1,
          "clipboard callback self-unmount leaked OTP owner");
    f.services.dispose();
}
} // namespace

int main() {
    try {
        navigation_ime_and_retirement();
        clipboard_and_eligibility();
        callback_and_formatter_reentry();
        initial_formatter_and_explicit_focus();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
