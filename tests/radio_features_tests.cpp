#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void key(Fixture& fixture, Key value, KeyAction action = KeyAction::down, bool repeat = false) {
    fixture.services.focus().dispatch({value, action, KeyModifier::none, repeat});
}

void click(Fixture& fixture, const detail::MountedSelectionComponent& item) {
    const auto bounds = fixture.nodes.require(item.node).bounds;
    const float x = bounds.x + bounds.width / 2;
    const float y = bounds.y + bounds.height / 2;
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
}

void dynamic_and_keyboard() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::vector<RadioOption>> options{std::vector<RadioOption>{
        {String{u8"a"}, String{u8"甲"}}, {42.0, String{u8"数字"}}, {true, String{u8"禁用"}, true}}};
    Signal<RadioDirection> direction{RadioDirection::LeftToRight};
    int calls{};
    host.mount(Content{[&] {
        Radio(RadioProps{}, RadioLabel{[] { Text(u8"Before"); }});
        RadioGroup(RadioGroupProps{}
                       .options(options)
                       .defaultValue(String{u8"a"})
                       .direction(direction)
                       .onValueChange([&](const RadioValue&) { ++calls; }));
        Radio(RadioProps{}, RadioLabel{[] { Text(u8"After"); }});
    }});
    fixture.synchronize();
    const auto before = host.mounted()[0];
    const auto a = host.mounted()[1];
    const auto number = host.mounted()[2];
    const auto disabled = host.mounted()[3];
    const auto after = host.mounted()[4];
    const auto group = host.radio_groups().front();
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == before.interaction, "Tab before failed");
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == a.interaction, "selected Tab entry failed");
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == number.interaction && calls == 1 &&
                host.radio_group_value(group) == RadioSelection{42.0},
            "typed arrow selection failed");
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == a.interaction && calls == 2, "arrow skip/wrap failed");
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == after.interaction, "group has multiple Tab stops");
    require(fixture.services.focus().request_focus(number.interaction, FocusModality::keyboard), "refocus failed");
    options.set(std::vector<RadioOption>{
        {42.0, String{u8"数字"}}, {String{u8"a"}, String{u8"甲"}}, {false, String{u8"新选项"}}});
    fixture.synchronize();
    require(host.mounted()[1].component == a.component && host.mounted()[2].surface == number.surface &&
                fixture.services.focus().state().focused == number.interaction &&
                !fixture.services.components().contains(disabled.component),
            "reorder replaced identity/focus");
    require(fixture.nodes.require(number.node).bounds.x < fixture.nodes.require(a.node).bounds.x,
            "options layout order did not update");
    direction.set(RadioDirection::RightToLeft);
    key(fixture, Key::left);
    require(fixture.services.focus().state().focused == a.interaction && calls == 2,
            "RTL arrow did not use logical order/current selection");
    click(fixture, number);
    require(calls == 3 && host.radio_group_value(group) == RadioSelection{42.0}, "number click failed");
    const auto bounds = fixture.nodes.require(number.node).bounds;
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                         bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    require(fixture.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "capture did not start");
    options.set(std::vector<RadioOption>{{false, String{u8"恢复"}}});
    fixture.synchronize();
    require(!fixture.services.pointer().state(PointerIdentity::mouse())->capture &&
                !fixture.services.focus().state().focused && !host.radio_group_value(group) && calls == 3,
            "deletion did not clear focus/capture/value silently");
    options.set(std::vector<RadioOption>{});
    fixture.synchronize();
    require(host.mounted().size() == 2, "empty options leaked members");
    options.set(std::vector<RadioOption>{{true, String{u8"恢复"}}});
    fixture.synchronize();
    click(fixture, host.mounted().back());
    require(host.radio_group_value(group) == RadioSelection{true} && calls == 4, "empty group did not recover");
}

void controlled_manual_and_callbacks() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<RadioSelection> selected{RadioSelection{String{u8"a"}}};
    Signal<bool> disabled{false};
    std::vector<int> order;
    int labels{};
    host.mount(Content{[&] {
        RadioGroup(
            RadioGroupProps{}.selection(selected).disabled(disabled).onValueChange([&](const RadioValue& value) {
                require(value == RadioValue{false}, "group candidate differed");
                order.push_back(2);
            }),
            RadioGroupContent{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         Radio(RadioProps{}.value(String{u8"a"}), RadioLabel{[&] {
                                   ++labels;
                                   Text(u8"A");
                               }});
                         Radio(RadioProps{}
                                   .value(false)
                                   .onChange([&](bool value) {
                                       require(value, "Radio member unchecked");
                                       order.push_back(1);
                                   })
                                   .onClick([&](bool value) {
                                       require(value, "Radio click candidate differed");
                                       order.push_back(3);
                                   }),
                               RadioLabel{[] { Text(u8"Bool"); }});
                     }});
                RadioGroup(
                    RadioGroupProps{}.options({{String{u8"a"}, String{u8"Nested"}}}).defaultValue(String{u8"a"}));
            }});
    }});
    fixture.synchronize();
    const auto a = host.mounted()[0];
    const auto boolean = host.mounted()[1];
    const auto nested = host.mounted()[2];
    click(fixture, boolean);
    require(order == std::vector<int>{1, 2, 3} && host.snapshot(a.component).checked &&
                !host.snapshot(boolean.component).checked && host.snapshot(nested.component).checked,
            "controlled candidate order or nearest group failed");
    selected.set(RadioSelection{false});
    require(host.snapshot(boolean.component).checked && !host.snapshot(a.component).checked && labels == 1 &&
                order.size() == 3,
            "external echo remounted/repeated");
    click(fixture, boolean);
    require(order == std::vector<int>{1, 2, 3, 3}, "checked Radio did not emit click only");
    disabled.set(true);
    require(host.snapshot(a.component).disabled && host.snapshot(boolean.component).disabled &&
                !host.snapshot(nested.component).disabled,
            "nested disabled isolation failed");
    disabled.set(false);
    selected.set(RadioSelection{String{u8"a"}});
    fixture.services.focus().request_focus(a.interaction, FocusModality::keyboard);
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == boolean.interaction && host.snapshot(a.component).checked,
            "controlled navigation did not retain authority/move focus");
}

void references_and_destruction() {
    RadioRef reference;
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<bool> disabled{false};
    int changes{};
    int clicks{};
    runtime::ComponentId id;
    host.mount(Content{[&] {
        Radio(RadioProps{}
                  .ref(reference)
                  .autoFocus(true)
                  .disabled(disabled)
                  .onChange([&](bool value) {
                      require(value, "standalone candidate false");
                      ++changes;
                      require(fixture.services.destroy(id), "self destroy failed");
                  })
                  .onClick([&](bool value) {
                      require(value, "copied click false");
                      ++clicks;
                  }));
    }});
    fixture.synchronize();
    id = host.mounted().front().component;
    require(reference.bound() && fixture.services.focus().state().focused == host.mounted().front().interaction,
            "Radio autoFocus/ref failed");
    require(reference.blur() && !reference.blur() && reference.focus(), "ref focus/blur failed");
    bool wrong_thread{};
    std::thread thread{[&] {
        try {
            static_cast<void>(reference.bound());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    }};
    thread.join();
    require(wrong_thread, "ref accepted wrong thread");
    disabled.set(true);
    require(!reference.focus() && !fixture.services.focus().state().focused, "disabled ref retained focus");
    disabled.set(false);
    require(reference.focus(), "ref did not recover");
    key(fixture, Key::space);
    key(fixture, Key::space, KeyAction::down, true);
    key(fixture, Key::space, KeyAction::up);
    require(changes == 1 && clicks == 1 && !reference.bound() && !reference.focus() && host.mounted().empty(),
            "self destroy callback/ref cleanup failed");
    Fixture recovery;
    detail::SelectionComponentHost recovery_host{recovery.services};
    recovery_host.mount(Content{[&] { Radio(RadioProps{}.ref(reference)); }});
    recovery.synchronize();
    require(reference.bound() && reference.focus(), "ref could not rebind after destroy");
}

template <class Declare> void rejected(Declare declare) {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    bool failed{};
    try {
        host.mount(Content{declare});
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed && host.mounted().empty() && fixture.services.interactions().size() == 0,
            "invalid Radio configuration leaked resources");
}

void invalid_and_reactive_rollback() {
    rejected([] { RadioGroup(RadioGroupProps{}.options({{1.0, String{u8"A"}}, {1.0, String{u8"B"}}})); });
    rejected([] { RadioGroup(RadioGroupProps{}.options({{std::numeric_limits<double>::infinity(), String{u8"A"}}})); });
    rejected([] { RadioGroup(RadioGroupProps{}.value(std::optional<String>{}).selection(RadioSelection{})); });
    rejected([] { RadioGroup(RadioGroupProps{}.selection(RadioSelection{}).defaultValue(false)); });
    rejected(
        [] { RadioGroup(RadioGroupProps{}.options({}), RadioGroupContent{[] { Radio(RadioProps{}.value(false)); }}); });
    rejected([] { RadioGroup(RadioGroupProps{}, RadioGroupContent{[] { Radio(RadioProps{}); }}); });
    rejected([] {
        RadioGroup(RadioGroupProps{}, RadioGroupContent{[] {
                       Radio(RadioProps{}.value(false));
                       Radio(RadioProps{}.value(false));
                   }});
    });
    rejected([] { Radio(RadioProps{}, RadioLabel{[] { Button(ButtonProps{}, [] { Text(u8"Interactive"); }); }}); });
    rejected([] {
        RadioRef ref;
        Radio(RadioProps{}.ref(ref));
        Radio(RadioProps{}.ref(ref));
    });
    std::vector<RadioOption> excessive;
    for (std::size_t i = 0; i < 1025; ++i) {
        excessive.push_back({double(i), String{u8"A"}});
    }
    rejected([&] { RadioGroup(RadioGroupProps{}.options(excessive)); });
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::vector<RadioOption>> options{std::vector<RadioOption>{{1.0, String{u8"A"}}}};
    host.mount(Content{[&] { RadioGroup(RadioGroupProps{}.options(options).defaultValue(1.0)); }});
    fixture.synchronize();
    const auto item = host.mounted().front();
    bool failed{};
    try {
        options.set(std::vector<RadioOption>{{2.0, String{u8"B"}}, {2.0, String{u8"C"}}});
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed && host.mounted().front().component == item.component && host.snapshot(item.component).checked,
            "invalid reactive options changed mounted state");
    options.set(std::vector<RadioOption>{{1.0, String{u8"A"}}, {false, String{u8"B"}}});
    fixture.synchronize();
    require(host.mounted().size() == 2 && host.mounted().front().component == item.component,
            "rollback did not recover");
}
} // namespace

int main() {
    try {
        dynamic_and_keyboard();
        controlled_manual_and_callbacks();
        references_and_destruction();
        invalid_and_reactive_rollback();
        std::cout << "radio features tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "radio features tests failed: " << error.what() << '\n';
        return 1;
    }
}
