#include "component/app_component.hpp"
#include "component/popconfirm_component.hpp"
#include "support/input_fixture.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    for (int i = 0; i < 3; ++i) {
        check(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "Popconfirm completion layout");
    }
}

void at(Fixture& f, std::int64_t time) {
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(time));
    sync(f);
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

void pointer(Fixture& f, PointerAction action, float x, float y, PointerButton button = PointerButton::primary) {
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), action, action == PointerAction::move ? PointerButton::none : button, x, y});
}

void press(Fixture& f, runtime::Rect r, PointerButton button = PointerButton::primary) {
    pointer(f, PointerAction::down, r.x + r.width / 2, r.y + r.height / 2, button);
    pointer(f, PointerAction::up, r.x + r.width / 2, r.y + r.height / 2, button);
    sync(f);
}

void hover() {
    Fixture f;
    Signal<PopconfirmTriggerMode> mode{PopconfirmTriggerMode::Hover};
    Signal<String> description{String{}};
    std::vector<bool> changes;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}
                       .title(u8"Hover confirmation")
                       .description(description)
                       .trigger(mode)
                       .onOpenChange([&](bool v) { changes.push_back(v); }),
                   PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Hover"); }); }});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted().front();
    const auto trigger = bounds(f, f.services.popconfirm().snapshot(id).trigger);
    pointer(f, PointerAction::move, trigger.x + 4, trigger.y + 4);
    at(f, 99999);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "hover opened before delay");
    at(f, 100000);
    auto s = f.services.popconfirm().snapshot(id);
    check(s.geometry.visible && changes == std::vector{true}, "hover deadline did not open");
    const auto& token = f.services.components().theme_scope(id)->snapshot().popconfirm();
    const auto compact_height = s.geometry.bounds.height;
    const auto description_id = s.description;
    check(std::abs(compact_height - token.padding * 2 - token.message_gap - token.line_height -
                   bounds(f, s.confirm).height) < .02F,
          "empty description retained a blank text row");
    description.set(String{u8"Additional explanation"});
    sync(f);
    check(std::abs(f.services.popconfirm().snapshot(id).geometry.bounds.height - compact_height - token.line_height -
                   token.description_gap) < .02F,
          "description did not add its actual row and spacing");
    description.set(String{});
    sync(f);
    s = f.services.popconfirm().snapshot(id);
    check(std::abs(s.geometry.bounds.height - compact_height) < .02F && s.description == description_id,
          "empty description did not restore compact geometry");
    pointer(f, PointerAction::move, s.geometry.bounds.x + 4, s.geometry.bounds.y + 4);
    at(f, 250000);
    check(f.services.popconfirm().snapshot(id).geometry.visible, "moving into popup closed it");
    pointer(f, PointerAction::move, 700, 550);
    at(f, 349999);
    check(f.services.popconfirm().snapshot(id).geometry.visible, "leave delay did not retain popup");
    at(f, 350000);
    check(!f.services.popconfirm().snapshot(id).geometry.visible && changes == std::vector({true, false}) &&
              !f.services.popconfirm().next_auxiliary_deadline(),
          "hover leave did not settle");
    mode.set(PopconfirmTriggerMode::HoverFocus);
    const auto focus = f.services.focus().state().focused;
    pointer(f, PointerAction::move, trigger.x + 4, trigger.y + 4);
    at(f, 450000);
    check(f.services.popconfirm().snapshot(id).geometry.visible && f.services.focus().state().focused == focus,
          "hover/focus combination stole keyboard focus on hover");
    pointer(f, PointerAction::move, 700, 550);
    at(f, 550000);
    check(!f.services.popconfirm().snapshot(id).geometry.visible,
          "hover/focus combination retained synthetic popup focus");
}

void context() {
    Fixture f;
    Signal<bool> disabled{false};
    Signal<PopconfirmTriggerMode> mode{PopconfirmTriggerMode::ContextMenu};
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.title(u8"Context confirmation").trigger(mode),
                   PopconfirmTrigger{[&] { Button(ButtonProps{}.disabled(disabled), [] { Text(u8"Context"); }); }});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted().front();
    const auto r = bounds(f, f.services.popconfirm().snapshot(id).trigger);
    const float x = r.x + 5;
    const float y = r.y + 5;
    pointer(f, PointerAction::down, x, y, PointerButton::secondary);
    disabled.set(true);
    sync(f);
    pointer(f, PointerAction::up, x, y, PointerButton::secondary);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "disabled native trigger accepted old context press");
    press(f, r, PointerButton::secondary);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "wrapper bypassed disabled native trigger");
    disabled.set(false);
    sync(f);
    pointer(f, PointerAction::up, x, y, PointerButton::secondary);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "context release had no matching press");
    press(f, r, PointerButton::secondary);
    check(f.services.popconfirm().snapshot(id).geometry.visible && !f.services.popconfirm().next_auxiliary_deadline(),
          "fresh context press did not open statically");
    press(f, {690, 540, 10, 10});
    pointer(f, PointerAction::down, x, y, PointerButton::secondary);
    mode.set(PopconfirmTriggerMode::Click);
    pointer(f, PointerAction::up, x, y, PointerButton::secondary);
    sync(f);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "changed trigger retained context ticket");
    press(f, r);
    check(f.services.popconfirm().snapshot(id).geometry.visible && !f.services.popconfirm().next_auxiliary_deadline(),
          "static click retained a redundant trigger timer");
}

void buttons() {
    Fixture f;
    Signal<bool> disabled{false};
    Signal<bool> loading{false};
    int clicked = 0;
    int confirmed = 0;
    bool load_on_click = false;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}
                       .title(u8"Forward props")
                       .defaultOpen()
                       .okButtonProps(ButtonProps{}
                                          .size(ControlSize::Large)
                                          .danger(true)
                                          .disabled(disabled)
                                          .loading(loading)
                                          .onClick([&] {
                                              ++clicked;
                                              if (load_on_click) {
                                                  loading.set(true);
                                              } else {
                                                  disabled.set(true);
                                              }
                                          }))
                       .cancelButtonProps(ButtonProps{}.disabled(true).shape(ButtonShape::Round))
                       .onConfirm([&] { ++confirmed; }),
                   PopconfirmTrigger{[] { Text(u8"Trigger"); }});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted().front();
    const auto s = f.services.popconfirm().snapshot(id);
    check(f.buttons.snapshot(s.confirm).size == ControlSize::Large &&
              f.buttons.snapshot(s.confirm).color == ButtonColor::Danger && f.buttons.snapshot(s.cancel).disabled &&
              f.buttons.snapshot(s.cancel).shape == ButtonShape::Round,
          "arbitrary button props were not forwarded");
    loading.set(true);
    sync(f);
    press(f, bounds(f, s.confirm));
    check(clicked == 0 && confirmed == 0, "forwarded loading allowed confirm");
    loading.set(false);
    sync(f);
    press(f, bounds(f, s.confirm));
    check(clicked == 1 && confirmed == 0 && f.services.popconfirm().snapshot(id).geometry.visible &&
              f.services.popconfirm().snapshot(id).confirm == s.confirm,
          "latest disabled or retained button failed");
    disabled.set(false);
    load_on_click = true;
    sync(f);
    press(f, bounds(f, s.confirm));
    check(clicked == 2 && confirmed == 0 && f.services.popconfirm().snapshot(id).geometry.visible,
          "button loading set by its own callback allowed internal confirmation");
    check(f.services.popconfirm().on_keyboard_input({Key::escape, KeyAction::down}), "Escape did not cancel");
    sync(f);
    check(!f.services.popconfirm().snapshot(id).geometry.visible, "disabled cancel button blocked Escape");
}

void motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    std::vector<bool> completed;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.open(open).animated().title(u8"Finite motion").afterOpenChange([&](bool v) {
            completed.push_back(v);
        }),
                   PopconfirmTrigger{[] { Text(u8"Trigger"); }});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted().front();
    const auto duration =
        animation::resolve_motion_tokens(f.services.components().theme_scope(id)->snapshot()).fast.count_microseconds();
    open.set(true);
    at(f, duration / 2);
    auto s = f.services.popconfirm().snapshot(id);
    check(s.geometry.visible && s.paint_visible && s.transitioning && std::abs(s.opacity - .875F) < .02F &&
              completed.empty(),
          "Popup animation midpoint or early completion");
    at(f, duration);
    check(completed == std::vector{true}, "entry completion missing");
    open.set(false);
    at(f, duration + duration / 2);
    s = f.services.popconfirm().snapshot(id);
    check(!s.geometry.visible && s.paint_visible && s.transitioning &&
              !f.services.interactions().component_active(s.confirm),
          "exit stopped painting or retained input");
    open.set(true);
    at(f, duration * 3);
    check(completed == std::vector({true, true}), "reversal delivered aborted close");
    open.set(false);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    check(completed == std::vector({true, true, false}) && !f.services.popconfirm().next_auxiliary_deadline(),
          "reduced motion retained close/deadline");
}

void rtl_retained() {
    Fixture f;
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    int mounts = 0;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.defaultOpen().title(u8"Retained RTL").direction(direction),
                   PopconfirmTrigger{[] { Text(u8"Trigger"); }}, {.description = PopconfirmDescription{[&] {
                                                                      ++mounts;
                                                                      Input(InputProps{}.defaultValue(u8"Draft"));
                                                                  }}});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted().front();
    const auto s = f.services.popconfirm().snapshot(id);
    const auto input = f.inputs.mounted_inputs().front();
    const auto padding = f.services.components().theme_scope(id)->snapshot().popconfirm().padding;
    const auto input_before = bounds(f, input.component);
    check(input_before.x >= s.geometry.bounds.x + padding - .02F &&
              input_before.x + input_before.width <= s.geometry.bounds.x + s.geometry.bounds.width - padding + .02F,
          "rich input overflows LTR message space");
    (void)f.services.focus().request_focus(input.interaction, FocusModality::keyboard);
    auto& edit = f.inputs.editors().require(input.editor);
    (void)edit.set_value("Dirty draft");
    (void)edit.select({8, 2});
    const auto session = f.inputs.sessions().active();
    (void)f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 1}, session});
    const auto selection = edit.selection();
    direction.set(TextDirection::RightToLeft);
    sync(f);
    const auto popup = f.services.popconfirm().snapshot(id).geometry.bounds;
    const auto input_after = bounds(f, input.component);
    check(input_after.x >= popup.x + padding - .02F &&
              input_after.x + input_after.width <= popup.x + popup.width - padding + .02F,
          "rich input overflows RTL message space");
    check(std::abs(bounds(f, s.confirm).x - popup.x - padding) < .02F &&
              bounds(f, s.confirm).x < bounds(f, s.cancel).x && mounts == 1 && edit.selection() == selection &&
              edit.composition().active && f.inputs.sessions().active() == session &&
              f.inputs.mounted_inputs().front().editor == input.editor,
          "RTL footer or retained rich editor failed");
}

void service() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    AppRef app;
    PopconfirmRef anchor;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Popconfirm(PopconfirmProps{}.serviceAnchor(anchor),
                           PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Anchor"); }); }});
            }});
    }});
    sync(f);
    int closed = 0;
    PopconfirmHandle panel;
    panel = app.popconfirm().confirm(
        anchor, PopconfirmProps{}.animated().title(u8"Service").afterOpenChange([&](bool shown) {
            if (!shown) {
                check(panel.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready,
                      "completion callback preceded service future");
                ++closed;
            }
        }));
    const auto id = f.services.popconfirm().mounted().back();
    const auto duration =
        animation::resolve_motion_tokens(f.services.components().theme_scope(id)->snapshot()).fast.count_microseconds();
    at(f, duration);
    check(panel.close(), "service close rejected");
    sync(f);
    check(panel.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready &&
              !panel.update(PopconfirmUpdate{.title = String{u8"Too late"}}),
          "service retired early or accepted closing update");
    at(f, duration * 2);
    check(panel.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready && closed == 1 &&
              anchor.bound() && f.services.popconfirm().mounted().size() == 1,
          "service animation terminal cleanup failed");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected Popconfirm completion case");
        const std::string_view name = argv[1];
        if (name == "hover") {
            hover();
        } else if (name == "context") {
            context();
        } else if (name == "buttons") {
            buttons();
        } else if (name == "motion") {
            motion();
        } else if (name == "rtl_retained") {
            rtl_retained();
        } else if (name == "service") {
            service();
        } else {
            throw std::runtime_error("unknown Popconfirm completion case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
