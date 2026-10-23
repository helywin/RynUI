#include "component/select_component.hpp"
#include "component/selection_component.hpp"
#include "component/slider_component.hpp"
#include "component/tabs_component.hpp"
#include "component/tooltip_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

runtime::ComponentId group(Fixture& f, Content content) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    build.mount_slot(id, std::move(content));
    return id;
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({500, 1200}, {0, 0, 500, 1200}), "boundary fixture layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    auto rect = node.bounds;
    rect.x += node.translation.x;
    rect.y += node.translation.y;
    return rect;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({key, KeyAction::down, modifiers});
    f.services.focus().dispatch({key, KeyAction::up, modifiers});
    sync(f);
}

void focus_refs_sessions_and_lifetime() {
    InputRef outside, inside, nested, dynamic;
    ButtonRef button;
    SwitchRef toggle;
    CheckboxRef checkbox;
    RadioRef radio;
    runtime::ComponentId outer, boundary, child;
    int activations{};
    Fixture f;
    detail::SelectionComponentHost selections{f.services};
    f.services.mount(Content{[&] {
        outer = group(f, Content{[&] {
                          Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                   Input(InputProps{}.defaultValue(u8"outside").ref(outside));
                                   Button(ButtonProps{}.ref(button).onClick([&] { ++activations; }),
                                          [] { Text(u8"Outside"); });
                                   Switch(SwitchProps{}.ref(toggle));
                                   Checkbox(CheckboxProps{}.ref(checkbox));
                                   Radio(RadioProps{}.ref(radio));
                               }});
                      }});
        boundary = group(f, Content{[&] {
                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                      Input(InputProps{}.defaultValue(u8"inside").ref(inside));
                                      Button(ButtonProps{}, [] { Text(u8"Inside"); });
                                      child = group(f, Content{[&] { Input(InputProps{}.ref(nested)); }});
                                  }});
                         }});
    }});
    sync(f);
    const auto editor = f.inputs.mounted_inputs()[0];
    check(outside.focus({InputFocusCursor::End}), "outside initial focus failed");
    const auto old_session = f.inputs.sessions().active();
    check(old_session.valid(), "outside input did not start session");
    const auto selected = f.inputs.editors().require(editor.editor).selection();
    const auto revision = f.services.interactions().input_boundary_revision();
    check(f.services.set_input_boundary(boundary), "new input boundary was not applied");
    sync(f);
    check(!f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
              f.services.components().branch_active(outer) &&
              !f.services.interactions().component_active(editor.component) &&
              f.services.interactions().require(editor.interaction).eligible,
          "boundary hid/disabled background or retained its focus/session");
    check(!outside.focus({InputFocusCursor::All}) && !outside.select(0, 2) && !button.focus() && !toggle.focus() &&
              !checkbox.focus() && !radio.focus() &&
              f.inputs.editors().require(editor.editor).selection().anchor == selected.anchor &&
              f.inputs.editors().require(editor.editor).selection().caret == selected.caret,
          "background ref reported success or mutated editor selection");
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"blocked"}, old_session})) &&
              f.inputs.editors().require(editor.editor).value() == String{u8"outside"}.bytes(),
          "old session edited the background");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(activations == 0, "background pointer activation passed through boundary");
    check(inside.focus(), "inside focus failed");
    for (int step = 0; step < 12; ++step) {
        key(f, Key::tab, step % 2 ? KeyModifier::shift : KeyModifier::none);
        const auto focus = f.services.focus().state().focused;
        check(focus && f.services.interactions().component_active(f.services.interactions().require(*focus).component),
              "Tab escaped boundary");
    }
    check(!f.services.set_input_boundary(boundary) &&
              f.services.interactions().input_boundary_revision() == revision + 1,
          "unchanged boundary changed its revision");
    f.services.append_slot(outer, Content{[&] { Input(InputProps{}.ref(dynamic).autoFocus()); }});
    sync(f);
    check(!dynamic.focus() && f.services.focus().state().focused != f.inputs.mounted_inputs().back().interaction,
          "new background autofocus escaped boundary");
    check(f.services.set_input_boundary(child) && !inside.focus() && nested.focus(), "nested boundary focus failed");
    check(f.services.set_input_boundary(boundary) && inside.focus(), "parent boundary did not resume");
    check(f.services.set_input_boundary(child) && f.services.destroy(boundary), "boundary owner destroy failed");
    sync(f);
    check(!f.services.interactions().input_boundary() && !inside.bound() && !nested.bound() && outside.focus() &&
              button.focus(),
          "retired boundary remained active or background refs did not resume");
    const auto generation = f.services.interactions().input_boundary_revision();
    bool rejected{};
    try {
        f.services.set_input_boundary(child);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && !f.services.interactions().input_boundary() &&
              f.services.interactions().input_boundary_revision() == generation,
          "stale boundary was accepted or partially published");
}

struct ScrollFallback final : detail::WindowComponentParticipant {
    explicit ScrollFallback(detail::WindowComponentServices& value) : services(value) {
        services.attach(*this);
    }

    ~ScrollFallback() override {
        services.detach(*this);
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    bool on_scroll_input(const ScrollInputEvent&) override {
        if (enabled) {
            ++calls;
            return true;
        }
        return false;
    }

    detail::WindowComponentServices& services;
    bool enabled{};
    int calls{};
};

std::vector<TabItem> long_tabs() {
    std::vector<TabItem> result;
    for (int i = 0; i < 8; ++i) {
        auto key = String::from_utf8(std::to_string(i)).value();
        result.emplace_back(key, String{u8"Long tab / 标签"}, TabContent{[] { Text(u8"Body"); }});
    }
    return result;
}

void popups_and_wheel_priority() {
    Signal<bool> select_open{true}, tip_open{true};
    std::vector<bool> select_requests, tip_requests;
    runtime::ComponentId boundary;
    Fixture f;
    ScrollFallback fallback{f.services};
    f.services.mount(Content{[&] {
        Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).open(select_open).onOpenChange([&](bool value) {
            select_requests.push_back(value);
        }));
        Tooltip(TooltipProps{}
                    .title(String{u8"Outside"})
                    .open(tip_open)
                    .trigger(TooltipTriggerMode::Manual)
                    .onOpenChange([&](bool value) { tip_requests.push_back(value); }),
                TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Outside tip"); }); }});
        Tabs(TabsProps{}.layout(LayoutStyle{}.width(dp(160))), long_tabs());
        boundary =
            group(f, Content{[&] {
                      Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                               TextArea(TextAreaProps{}
                                            .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight")
                                            .layout(LayoutStyle{}.width(dp(180)).height(dp(64))));
                               Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).showSearch());
                               Tabs(TabsProps{}.layout(LayoutStyle{}.width(dp(160))), long_tabs());
                               Tooltip(TooltipProps{}.title(String{u8"Inside"}).trigger(TooltipTriggerMode::Click),
                                       TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Inside tip"); }); }});
                           }});
                  }});
    }});
    sync(f);
    const auto outside_select = f.services.select().mounted()[0];
    const auto inside_select = f.services.select().mounted()[1];
    const auto outside_tip = f.services.tooltip().mounted()[0];
    const auto inside_tip = f.services.tooltip().mounted()[1];
    check(f.services.select().snapshot(outside_select).visible && f.services.tooltip().snapshot(outside_tip).visible,
          "initial controlled popup fixture failed");
    f.services.set_input_boundary(boundary);
    sync(f);
    check(!f.services.select().snapshot(outside_select).visible &&
              !f.services.tooltip().snapshot(outside_tip).visible && select_open.get() && tip_open.get() &&
              select_requests == std::vector<bool>{false} && tip_requests == std::vector<bool>{false},
          "boundary retained an outside popup or changed controlled values");
    const auto outside_tabs = f.services.tabs().mounted()[0];
    const auto outside_nav = f.services.tabs().snapshot(outside_tabs);
    check(f.inputs.dispatch(ScrollInputEvent{100, 0, outside_nav.viewport.x + 5, outside_nav.viewport.y + 5}),
          "modal boundary did not consume background wheel");
    sync(f);
    check(f.services.tabs().snapshot(outside_tabs).scroll == 0, "background Tabs scrolled through boundary");
    const auto area = f.inputs.mounted_inputs()[1].component;
    const auto viewport = f.inputs.layout_snapshot(area).viewport;
    fallback.enabled = true;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, viewport.x + 5, viewport.y + 5}),
          "inside TextArea wheel unhandled");
    fallback.enabled = false;
    sync(f);
    check(f.inputs.layout_snapshot(area).vertical_scroll > 0 && fallback.calls == 0, "container stole TextArea wheel");
    click(f, f.services.select().snapshot(inside_select).selector);
    check(f.services.select().snapshot(inside_select).visible && f.inputs.sessions().active().valid(),
          "inside Select did not open/start search");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, f.inputs.sessions().active()})),
          "inside preedit failed");
    key(f, Key::escape);
    check(f.services.select().snapshot(inside_select).visible, "popup consumed IME Escape first");
    key(f, Key::escape);
    check(!f.services.select().snapshot(inside_select).visible &&
              f.services.interactions().input_boundary() == boundary,
          "Select Escape escaped input boundary");
    const auto inside_tabs = f.services.tabs().mounted()[1];
    const auto nav = f.services.tabs().snapshot(inside_tabs);
    check(f.inputs.dispatch(ScrollInputEvent{1, 0, nav.viewport.x + 5, nav.viewport.y + 5}),
          "inside Tabs wheel unhandled");
    sync(f);
    check(f.services.tabs().snapshot(inside_tabs).scroll > 0, "inside Tabs did not scroll");
    click(f, f.buttons.mounted_buttons()[1].component);
    check(f.services.tooltip().snapshot(inside_tip).visible, "inside Tooltip did not open");
    key(f, Key::escape);
    check(!f.services.tooltip().snapshot(inside_tip).visible, "inside Tooltip did not consume Escape");
}

void capture_and_reentrant_boundaries() {
    runtime::ComponentId boundary;
    bool constrain{};
    int completed{};
    SliderRef reference;
    Fixture f;
    f.services.mount(Content{[&] {
        Slider(SliderProps{}
                   .defaultValue(20)
                   .ref(reference)
                   .layout(LayoutStyle{}.width(dp(240)))
                   .onChange([&](double) {
                       if (constrain) {
                           f.services.set_input_boundary(boundary);
                       }
                   })
                   .onChangeComplete([&](double) { ++completed; }));
        boundary = group(f, Content{[] { Input(InputProps{}); }});
    }});
    sync(f);
    const auto slider = f.services.slider().mounted()[0].component;
    const auto press = [&] {
        const auto center = f.services.slider().snapshot(slider).centers[0];
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, center.x, center.y});
        return center;
    };
    static_cast<void>(press());
    check(f.services.slider().snapshot(slider).dragging, "Slider did not begin capture");
    f.services.set_input_boundary(boundary);
    sync(f);
    check(!f.services.slider().snapshot(slider).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture && !reference.focus() && completed == 0,
          "boundary retained Slider gesture/capture/ref or completed a cancelled gesture");
    f.services.set_input_boundary({});
    check(reference.focus(), "Slider ref did not resume");
    constrain = true;
    const auto center = press();
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, center.x + 50, center.y});
    sync(f);
    check(f.services.interactions().input_boundary() == boundary && !f.services.slider().snapshot(slider).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture && completed == 0,
          "boundary opened inside drag callback retained gesture state");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, center.x + 50, center.y});
    sync(f);
    check(completed == 0, "release after boundary cancellation completed old drag");
}

void blur_reentry_and_keyboard_cleanup() {
    for (int mode = 0; mode < 2; ++mode) {
        runtime::ComponentId a, b;
        InputRef outside, first, second;
        bool act{};
        int blurred{};
        Fixture f;
        f.services.mount(Content{[&] {
            Input(InputProps{}.ref(outside).onBlur([&] {
                ++blurred;
                if (std::exchange(act, false)) {
                    if (mode == 0) {
                        f.services.set_input_boundary(b);
                    } else {
                        static_cast<void>(f.services.destroy(a));
                    }
                }
            }));
            a = group(f, Content{[&] { Input(InputProps{}.ref(first)); }});
            b = group(f, Content{[&] { Input(InputProps{}.ref(second)); }});
        }});
        sync(f);
        check(outside.focus(), "reentry fixture focus failed");
        act = true;
        f.services.set_input_boundary(a);
        sync(f);
        check(blurred == 1 && !f.inputs.sessions().active().valid(), "boundary blur reentry leaked input session");
        if (mode == 0) {
            check(f.services.interactions().input_boundary() == b && !first.focus() && second.focus(),
                  "outer boundary overwrote callback transfer");
        } else {
            check(!f.services.interactions().input_boundary() && !first.bound() && outside.focus(),
                  "destroyed boundary did not clear safely");
        }
    }
    Signal<bool> open{true};
    runtime::ComponentId boundary;
    int blurred{};
    Fixture f;
    f.services.mount(Content{[&] {
        Search(SearchProps{}
                   .defaultValue(u8"go")
                   .onSearch([&](String, SearchSource) { f.services.set_input_boundary(boundary); })
                   .onBlur([&] { ++blurred; }));
        Tooltip(TooltipProps{}.title(String{u8"Open outside"}).open(open).trigger(TooltipTriggerMode::Manual),
                TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Tip"); }); }});
        boundary = group(f, Content{[] { Input(InputProps{}); }});
    }});
    sync(f);
    check(f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard),
          "Search focus failed");
    check(f.inputs.sessions().active().valid(), "Search session missing");
    key(f, Key::enter);
    check(f.services.interactions().input_boundary() == boundary && blurred == 1 &&
              !f.inputs.sessions().active().valid() &&
              !f.services.tooltip().snapshot(f.services.tooltip().mounted()[0]).visible,
          "keyboard boundary/Tooltip cleanup silently removed focus without stopping IME");
}

void delayed_tip_does_not_survive_boundary() {
    runtime::ComponentId boundary;
    Fixture f;
    f.services.mount(Content{[&] {
        Tooltip(TooltipProps{}
                    .title(String{u8"Delayed"})
                    .trigger(TooltipTriggerMode::Hover)
                    .mouseEnterDelay(Duration::milliseconds(100)),
                TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Hover"); }); }});
        boundary = group(f, Content{[] { Button(ButtonProps{}, [] { Text(u8"Inside"); }); }});
    }});
    sync(f);
    const auto rect = bounds(f, f.buttons.mounted_buttons()[0].component);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, rect.x + 5, rect.y + 5});
    sync(f);
    check(f.services.next_frame_deadline().has_value(), "Tooltip delay was not scheduled");
    f.services.set_input_boundary(boundary);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1'000'000)));
    sync(f);
    const auto tip = f.services.tooltip().mounted()[0];
    check(!f.services.tooltip().snapshot(tip).visible && !f.services.next_frame_deadline(),
          "outside Tooltip deadline survived boundary");
    f.services.set_input_boundary({});
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(2'000'000)));
    sync(f);
    check(!f.services.tooltip().snapshot(tip).visible, "stale hover reopened Tooltip after boundary");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        focus_refs_sessions_and_lifetime();
        popups_and_wheel_priority();
        capture_and_reentrant_boundaries();
        blur_reentry_and_keyboard_cleanup();
        delayed_tip_does_not_survive_boundary();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
