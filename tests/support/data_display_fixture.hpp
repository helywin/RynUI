#pragma once
#include "input_fixture.hpp"
#include "component/collapse_component.hpp"
#include "component/descriptions_component.hpp"
#include "component/segmented_component.hpp"
#include "component/timeline_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "component/form_component.hpp"
#include <iostream>

namespace data_display_test {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

inline void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

inline void sync(Fixture& f, float width = 600, float height = 1000) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "DataDisplay layout failed");
}

inline runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

inline void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

inline void key(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({key, KeyAction::down, modifiers});
    f.services.focus().dispatch({key, KeyAction::up, modifiers});
    sync(f);
}

inline void tick(Fixture& f, std::int64_t micros) {
    const auto time = animation::AnimationTime::microseconds(micros);
    f.services.set_animation_time(time);
    static_cast<void>(f.services.tick_animations(time));
    sync(f);
}

inline void draft(Fixture& f, InputRef ref, String value) {
    check(ref.focus({InputFocusCursor::All}), "Input ref lost focus eligibility");
    check(bool(f.inputs.dispatch(TextCommitted{value, f.inputs.sessions().active()})), "Input commit failed");
    sync(f);
}

inline std::array<float, 4> text_color(Fixture& f, std::string_view content) {
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto& state = f.scene.text_state(text.scene);
        if (state.content().bytes() == content) {
            return state.material().color;
        }
    }
    throw std::runtime_error("Required styled scene text is missing");
}

inline std::array<float, 4> channels(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

inline void form_composition(int widget) {
    Fixture f;
    FormRef form;
    FormField<String> field{String{u8"initial"}};
    InputRef ref;
    int finishes{};
    const auto content = [&] {
        Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"name"}, field),
                          FormItemContent{[&] { Input(InputProps{}.ref(ref)); }});
                 Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
                 Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click),
                         PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Popup"); }); }},
                         PopoverSlots{std::nullopt, PopoverContent{[] {
                                          Select(SelectProps{}.options({{1, u8"Nested"}, {2, u8"Two"}}));
                                      }}});
             }});
    };
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++finishes; }), FormContent{[&] {
                 if (widget == 0) {
                     Collapse(CollapseProps{}.defaultActiveKeys({u8"a"}),
                              {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"Header"); }}, CollapseBody{content}}});
                 }
                 if (widget == 1) {
                     Segmented(SegmentedProps{}, {SegmentedOption{u8"a", u8"A"}.content(SegmentedLabel{content})});
                 }
                 if (widget == 2) {
                     Descriptions(DescriptionsProps{}.column(1),
                                  {DescriptionsItem{u8"a", DescriptionsLabel{[] { Text(u8"Label"); }},
                                                    DescriptionsContent{content}}});
                 }
                 if (widget == 3) {
                     Timeline(TimelineProps{}, {TimelineItem{u8"a", u8""}.richContent(TimelineContent{content})});
                 }
             }});
    }});
    sync(f);
    draft(f, ref, u8"bound native field");
    check(field.get() == String{u8"bound native field"}, "rich nested FormItem did not bind automatically");
    key(f, Key::enter);
    check(finishes == 1, "container stole its rich field's Enter submission");
    check(form.reset() && field.get() == String{u8"initial"}, "Form reset did not reach retained rich field");
    sync(f);
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "rich child Select did not open");
    key(f, Key::escape);
    check(!f.services.select().snapshot(select).visible, "rich child Select lost Escape ownership");
    check(ref.focus(), "rich input did not regain focus");
    const auto input_hit = f.services.focus().state().focused;
    key(f, Key::tab);
    key(f, Key::tab, KeyModifier::shift);
    check(f.services.focus().state().focused == input_hit, "Tab/ShiftTab did not preserve native rich focus order");
    const auto popover = f.services.popover().mounted()[0];
    click(f, f.buttons.mounted_buttons()[0].component);
    check(f.services.popover().snapshot(popover).geometry.visible, "rich child Popover did not open");
    const auto nested = f.services.select().mounted().back();
    click(f, f.services.select().snapshot(nested).selector);
    check(f.services.select().snapshot(nested).visible, "nested Select did not open");
    key(f, Key::escape);
    check(!f.services.select().snapshot(nested).visible && f.services.popover().snapshot(popover).geometry.visible,
          "outer container stole innermost popup Escape");
    key(f, Key::escape);
    check(!f.services.popover().snapshot(popover).geometry.visible, "second Escape did not close containing Popover");
    const auto id = f.services.form().mounted()[0];
    check(f.services.destroy(id) && !ref.focus() && f.services.select().mounted().empty() &&
              f.services.popover().mounted().empty(),
          "Form composition leaked popup or field owners");
}

} // namespace data_display_test
