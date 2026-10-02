#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <class Function> void rejected(Function function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Checkbox invalid input was accepted");
}

void activate(Fixture& f, input::InteractionId id) {
    require(f.services.focus().request_focus(id, input::FocusModality::keyboard), "Checkbox focus failed");
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
}

void controlled_and_values() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<CheckboxValues> value{CheckboxValues{String{u8"one"}}};
    Signal<bool> disabled{false};
    CheckboxValues candidate;
    int changes{};
    f.buttons.mount(Content{[&] {
        CheckboxGroup(CheckboxGroupProps{}
                          .options(std::vector<CheckboxOption>{{String{u8"one"}, String{u8"文字"}},
                                                               {1.0, String{u8"数字"}},
                                                               {true, String{u8"布尔"}, true}})
                          .value(value)
                          .disabled(disabled)
                          .onChange([&](const CheckboxValues& next) {
                              candidate = next;
                              ++changes;
                          }));
        Switch(SwitchProps{});
        Radio(RadioProps{});
    }});
    f.synchronize();
    const auto first = host.mounted()[0];
    const auto second = host.mounted()[1];
    const auto third = host.mounted()[2];
    require(host.checkbox_groups().size() == 1 && host.mounted().size() == 5 && host.snapshot(first.component).checked,
            "Checkbox typed Group inventory/value incorrect");
    const auto mounts = f.services.components().mount_runs();
    const auto group = host.checkbox_groups()[0];
    require(!f.services.interactions().require(group.interaction).focusable, "CheckboxGroup is a Tab target");
    activate(f, second.interaction);
    require(changes == 1 && candidate == CheckboxValues{String{u8"one"}, 1.0} &&
                !host.snapshot(second.component).checked,
            "Checkbox controlled Group did not preserve authority/candidate order");
    value.set(candidate);
    f.synchronize();
    require(host.snapshot(second.component).checked && changes == 1 && f.services.components().mount_runs() == mounts,
            "Checkbox external controlled update remounted or emitted change");
    disabled.set(true);
    f.synchronize();
    require(host.snapshot(first.component).disabled && host.snapshot(second.component).disabled &&
                host.snapshot(third.component).disabled,
            "Checkbox Group disabled did not propagate");
    disabled.set(false);
    f.synchronize();
    require(!host.snapshot(first.component).disabled && host.snapshot(third.component).disabled,
            "Checkbox own disabled was lost after Group enabled");
    rejected([&] { value.set({1.0, 1.0}); });
    require(host.checkbox_group_value(group.component) == candidate && host.snapshot(second.component).checked,
            "Checkbox invalid controlled value mutated published state");
    value.set({true});
    f.synchronize();
    require(host.snapshot(third.component).checked && !host.snapshot(first.component).checked && changes == 1,
            "Checkbox disabled value cannot be set externally");
}

void dynamic_options() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<std::vector<CheckboxOption>> options{
        std::vector<CheckboxOption>{{String{u8"a"}, String{u8"A"}}, {String{u8"b"}, String{u8"B"}}}};
    int changes{};
    CheckboxValues candidate;
    f.buttons.mount(Content{[&] {
        CheckboxGroup(CheckboxGroupProps{}
                          .options(options)
                          .defaultValue({String{u8"b"}})
                          .onChange([&](const CheckboxValues& next) {
                              candidate = next;
                              ++changes;
                          }));
        Checkbox(CheckboxProps{}, CheckboxLabel{[] { Text(u8"following control"); }});
    }});
    f.synchronize();
    const auto a = host.mounted()[0];
    const auto b = host.mounted()[1];
    const auto following = host.mounted()[2];
    const auto group = host.checkbox_groups()[0];
    const auto label = f.services.text().mounted_texts()[0];
    const auto label_shapes = f.scene.text_state(label.scene).counters().shape_count;
    require(f.services.focus().request_focus(b.interaction, input::FocusModality::keyboard),
            "Checkbox dynamic focus failed");
    options.set({{String{u8"b"}, String{u8"B updated"}}, {String{u8"a"}, String{u8"A"}}, {2.0, String{u8"C"}}});
    f.synchronize();
    const auto c = host.mounted()[3];
    require(host.mounted()[0].component == a.component && host.mounted()[1].component == b.component &&
                f.services.focus().state().focused == b.interaction &&
                f.nodes.require(b.node).bounds.x < f.nodes.require(a.node).bounds.x &&
                f.scene.text_state(label.scene).counters().shape_count == label_shapes,
            "Checkbox options reorder remounted unchanged options/labels or lost focus");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == a.interaction, "Checkbox options Tab order did not follow data");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == c.interaction,
            "Checkbox new option appended after following controls");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == following.interaction, "Checkbox Group escaped keyboard boundary");
    activate(f, a.interaction);
    require(candidate == CheckboxValues{String{u8"b"}, String{u8"a"}} && changes == 1,
            "Checkbox candidate ordering incorrect");
    rejected([&] { options.set({{String{u8"a"}, String{u8"wrong"}}, {String{u8"a"}, String{u8"duplicate"}}}); });
    require(host.mounted().size() == 4 && host.snapshot(b.component).checked,
            "Checkbox invalid options replaced old scene");
    const auto bounds = f.nodes.require(a.node).bounds;
    f.services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::down,
                                   input::PointerButton::primary, bounds.x + bounds.width / 2,
                                   bounds.y + bounds.height / 2});
    require(f.services.pointer().state(input::PointerIdentity::mouse())->capture == a.interaction,
            "Checkbox option pointer capture failed");
    options.set({{String{u8"b"}, String{u8"B updated"}}, {2.0, String{u8"C"}, true}});
    f.synchronize();
    require(!f.services.components().contains(a.component) && !f.services.focus().state().focused &&
                host.checkbox_group_value(group.component) == CheckboxValues{String{u8"b"}} &&
                host.snapshot(c.component).disabled && changes == 1,
            "Checkbox removed selected/focused option retained resources/value or emitted change");
    require(!f.services.pointer().state(input::PointerIdentity::mouse())->capture,
            "Checkbox removed option retained pointer capture");
    options.set({});
    f.synchronize();
    require(host.mounted().size() == 1 && host.checkbox_group_value(group.component).empty() &&
                f.services.interactions().size() == 2,
            "Checkbox empty Group leaked option interactions");
    options.set({{false, String{u8"false"}}});
    f.synchronize();
    require(host.mounted().size() == 2, "Checkbox empty options did not recover");
}

void manual_content_and_lifetime() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> disabled{false};
    int content_runs{};
    int label_runs{};
    int own_changes{};
    int group_changes{};
    f.buttons.mount(Content{[&] {
        CheckboxGroup(CheckboxGroupProps{}.disabled(disabled).onChange([&](const CheckboxValues& value) {
            require(value == CheckboxValues{true}, "Checkbox copied Group callback candidate wrong");
            ++group_changes;
        }),
                      CheckboxGroupContent{[&] {
                          ++content_runs;
                          Space(SpaceProps{}, SpaceContent{[&] {
                                    Checkbox(CheckboxProps{}.value(true).onChange([&](bool value) {
                                        require(value, "Checkbox own candidate false");
                                        ++own_changes;
                                        f.services.destroy(host.checkbox_groups().back().component);
                                    }),
                                             CheckboxLabel{[&] {
                                                 ++label_runs;
                                                 Icon(IconProps{}.name(IconName::CheckOutlined));
                                                 Text(u8"rich");
                                             }});
                                    Checkbox(CheckboxProps{}.skipGroup(true).defaultChecked(true));
                                    CheckboxGroup(CheckboxGroupProps{}.options(
                                        std::vector<CheckboxOption>{{1.0, String{u8"nested"}}}));
                                }});
                      }});
    }});
    f.synchronize();
    const auto item = host.mounted()[0];
    require(host.checkbox_groups().size() == 2 && host.mounted().size() == 3 &&
                host.snapshot(host.mounted()[1].component).checked,
            "Checkbox nearest Group/skipGroup failed");
    disabled.set(true);
    f.synchronize();
    require(host.snapshot(item.component).disabled && !host.snapshot(host.mounted()[1].component).disabled &&
                !host.snapshot(host.mounted()[2].component).disabled && content_runs == 1 && label_runs == 1,
            "Checkbox Group propagated into skip/nested Group or reran rich content");
    disabled.set(false);
    f.synchronize();
    activate(f, item.interaction);
    require(own_changes == 1 && group_changes == 1 && host.mounted().empty() && host.checkbox_groups().empty() &&
                f.services.interactions().size() == 0 && f.scene.size() == 0,
            "Checkbox callback destruction leaked state");
}

void invalid_mounts() {
    for (int kind = 0; kind < 8; ++kind) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        rejected([&] {
            f.buttons.mount(Content{[&] {
                if (kind == 0) {
                    CheckboxGroup(CheckboxGroupProps{}.options(
                        std::vector<CheckboxOption>{{std::numeric_limits<double>::quiet_NaN(), String{u8"bad"}}}));
                } else if (kind == 1) {
                    CheckboxGroup(CheckboxGroupProps{}.value(CheckboxValues{}).defaultValue({true}));
                } else if (kind == 2) {
                    CheckboxGroup(CheckboxGroupProps{}.options(std::vector<CheckboxOption>{}),
                                  CheckboxGroupContent{[] {}});
                } else if (kind == 3) {
                    CheckboxGroup(CheckboxGroupProps{}, CheckboxGroupContent{[] { Checkbox(CheckboxProps{}); }});
                } else if (kind == 4) {
                    Checkbox(CheckboxProps{},
                             CheckboxLabel{[] { Button(ButtonProps{}, [] { Text(u8"interactive"); }); }});
                } else if (kind == 5) {
                    CheckboxGroup(CheckboxGroupProps{}.defaultValue({true, true}));
                } else if (kind == 6) {
                    CheckboxGroup(CheckboxGroupProps{}, CheckboxGroupContent{[] {
                                      Checkbox(CheckboxProps{}.value(true));
                                      Checkbox(CheckboxProps{}.value(true));
                                  }});
                } else {
                    CheckboxGroup(CheckboxGroupProps{}.options(std::vector<CheckboxOption>(1025)));
                }
            }});
        });
        require(f.nodes.size() == 0 && f.services.interactions().size() == 0 && host.mounted().empty() &&
                    host.checkbox_groups().empty() && f.scene.size() == 0,
                "Checkbox invalid mount leaked transaction resources");
    }
}
} // namespace

int main() {
    try {
        controlled_and_values();
        dynamic_options();
        manual_content_and_lifetime();
        invalid_mounts();
        std::cout << "Checkbox Group contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
