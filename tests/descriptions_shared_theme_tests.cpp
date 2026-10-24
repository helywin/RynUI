#include "component/descriptions_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

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
    check(f.services.layout_and_synchronize({600, 700}, {0, 0, 600, 700}), "merged Theme layout failed");
}

void shared_theme_and_picker_boundary() {
    Fixture f;
    Signal<ThemeConfig> config{detail::default_theme_config()};
    Signal<bool> shown{true};
    Signal<bool> open{false};
    TimePickerRef ref;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Descriptions(DescriptionsProps{}.column(1).bordered(),
                               {DescriptionsItem{
                                   u8"time", DescriptionsLabel{[] { Text(u8"Time"); }}, DescriptionsContent{[&] {
                                       ++mounts;
                                       TimePicker(TimePickerProps{}.value(TimeOfDay{9, 10, 11}).open(open).ref(ref));
                                   }}}
                                    .visible(shown)});
              }});
    }});
    sync(f);
    const auto descriptions = f.services.descriptions().mounted()[0];
    const auto picker = f.services.time_picker().mounted()[0];
    const auto original = f.services.descriptions().snapshot(descriptions);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto scope = f.services.components().theme_scope(descriptions);
    check(scope == f.services.components().theme_scope(picker), "Time/Descriptions did not share the declared Theme");
    int description_colors = 0;
    int time_colors = 0;
    int description_metrics = 0;
    int time_metrics = 0;
    auto dc = scope->capture([&](theme_runtime::DirtyPhase) { ++description_colors; },
                             [&] { static_cast<void>(scope->descriptions_colors()); });
    auto tc = scope->capture([&](theme_runtime::DirtyPhase) { ++time_colors; },
                             [&] { static_cast<void>(scope->time_picker_colors()); });
    auto dm = scope->capture([&](theme_runtime::DirtyPhase) { ++description_metrics; },
                             [&] { static_cast<void>(scope->descriptions_metrics()); });
    auto tm = scope->capture([&](theme_runtime::DirtyPhase) { ++time_metrics; },
                             [&] { static_cast<void>(scope->time_picker_metrics()); });
    check(ref.focus({InputFocusCursor::All}), "nested Time input focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"09:"}, f.inputs.sessions().active()})),
          "nested Time draft failed");
    sync(f);
    const auto session = f.inputs.sessions().active();
    auto next = std::make_shared<ThemeConfig>(detail::default_theme_config());
    next->descriptions.tokens.bordered_label = Color::rgba8(20, 70, 100);
    next->time_picker.tokens.text = Color::rgba8(100, 70, 20);
    config.set(*next);
    sync(f);
    check(description_colors == 1 && time_colors == 1 && description_metrics == 0 && time_metrics == 0,
          "merged color identities collided or changed metric subscribers");
    check(f.inputs.sessions().active() == session && f.inputs.display_snapshot(input.component).text == "09:",
          "simultaneous Theme colors reset active Time draft/session");
    next->descriptions.tokens.padding_inline = dp(20);
    next->time_picker.tokens.column_width = dp(64);
    config.set(*next);
    sync(f);
    check(description_colors == 1 && time_colors == 1 && description_metrics == 1 && time_metrics == 1,
          "merged metric identities collided");
    check(f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.display_snapshot(input.component).text == "09:",
          "metric update remounted or reset nested Time input");
    open.set(true);
    sync(f);
    const auto popup = f.services.time_picker().snapshot(picker);
    const auto value_node = f.services.components().root(original.items[0].content);
    const auto& value_bounds = f.nodes.require(value_node).bounds;
    const auto popup_node = f.services.components().root(popup.popup);
    const auto popup_clip = f.nodes.content_clip(popup_node, {0, 0, 600, 700});
    check(popup.visible && f.nodes.require(popup_node).clip_boundary && popup.bounds.height > value_bounds.height &&
              popup_clip.y + popup_clip.height > value_bounds.y + value_bounds.height,
          "slot content clip cropped the independent Time popup boundary");
    shown.set(false);
    sync(f);
    check(!f.services.time_picker().snapshot(picker).visible && !ref.focus(), "hidden slot retained Time popup/input");
    shown.set(true);
    sync(f);
    check(!f.services.time_picker().snapshot(picker).visible && mounts == 1 &&
              f.services.descriptions().snapshot(descriptions).items[0].content == original.items[0].content,
          "restoring slot resurrected popup or remounted Time content");
}
} // namespace

int main() {
    try {
        shared_theme_and_picker_boundary();
        std::cout << "Descriptions/Time shared Theme and clip boundaries passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
