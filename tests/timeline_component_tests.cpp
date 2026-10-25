#include "support/timeline_fixture.hpp"
#include "component/select_component.hpp"
#include <ryn/flex.hpp>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void retained_layout() {
    Fixture f;
    Signal<TimelineMode> mode{TimelineMode::Start};
    Signal<bool> reverse{false};
    Signal<bool> shown{true};
    Signal<String> title{String{u8"Monday"}};
    Signal<String> content{String{u8"Event content"}};
    Signal<TimelineTone> tone{TimelineTone::Red};
    Signal<bool> loading{false};
    InputRef ref;
    int mounts = 0;
    int clicks = 0;
    f.services.mount(Content{[&] {
        Timeline(
            TimelineProps{}.mode(mode).reverse(reverse).layout(LayoutStyle{}.width(dp(580))),
            {TimelineItem{u8"first", content}.title(title).tone(tone).loading(loading).richContent(TimelineContent{[&] {
                 ++mounts;
                 Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
             }}),
             TimelineItem{u8"second", u8""}
                 .title(u8"Tuesday")
                 .placement(TimelinePlacement::End)
                 .richContent(TimelineContent{[&] {
                     ++mounts;
                     Button(ButtonProps{}.wave(false).onClick([&] { ++clicks; }), [] { Text(u8"Act"); });
                 }})
                 .visible(shown),
             TimelineItem{u8"third", u8""}.title(u8"Wednesday").richContent(TimelineContent{[&] {
                 ++mounts;
                 Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
             }})});
    }});
    sync(f);
    const auto id = f.services.timeline().mounted()[0];
    const auto initial = f.services.timeline().snapshot(id);
    check(initial.order == std::vector<detail::TimelineModelEntry>{{0, TimelinePlacement::Start},
                                                                   {1, TimelinePlacement::End},
                                                                   {2, TimelinePlacement::Start}},
          "initial placement drifted");
    check(bounds(f, initial.items[0].title).x < bounds(f, initial.items[0].content).x &&
              bounds(f, initial.items[1].title).x > bounds(f, initial.items[1].content).x,
          "actual title/content sides missing");
    check(initial.items[0].tail.height > 0 && initial.items[2].tail.height == 0, "actual tail geometry missing");
    check(!f.services.surfaces().instances().instances().empty() &&
              !f.services.rounded_effects().packed_instances().empty() && f.scene.glyph_scene().instances().size() > 0,
          "Timeline emitted no real scene");
    const auto input = f.inputs.mounted_inputs()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto select = f.services.select().mounted()[0];
    click(f, button.component);
    check(clicks == 1, "passive Timeline blocked Button");
    check(ref.focus({InputFocusCursor::All}), "rich Input did not focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved draft"}, f.inputs.sessions().active()})),
          "draft commit failed");
    const auto session = f.inputs.sessions().active();
    const auto count = f.services.components().component_count();
    reverse.set(true);
    mode.set(TimelineMode::Alternate);
    title.set(u8"Updated title");
    content.set(u8"Updated content");
    tone.set(TimelineTone::Green);
    loading.set(true);
    sync(f);
    const auto reversed = f.services.timeline().snapshot(id);
    check(reversed.order.front().item == 2 && reversed.order.back().item == 0 && reversed.items[0].tail.height == 0 &&
              reversed.items[2].tail.height > 0,
          "reverse left a stale tail/order");
    check(bounds(f, initial.items[2].viewport).y < bounds(f, initial.items[0].viewport).y,
          "reverse did not place retained nodes");
    check(f.inputs.sessions().active() == session && f.inputs.editors().require(input.editor).value() == "Saved draft",
          "ordinary updates reset editor/session");
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "Button focus failed");
    f.services.focus().dispatch({Key::tab, KeyAction::down});
    check(f.services.focus().state().focused == input.interaction, "reverse did not reorder actual Tab traversal");
    shown.set(false);
    sync(f);
    check(f.services.timeline().snapshot(id).order.size() == 2, "hidden item retained row");
    shown.set(true);
    mode.set(TimelineMode::End);
    reverse.set(false);
    sync(f);
    check(f.inputs.mounted_inputs()[0].editor == input.editor && f.services.select().mounted()[0] == select &&
              f.services.components().component_count() == count && mounts == 3,
          "ordinary update remounted rich controls");
    const auto order = f.services.timeline().snapshot(id).order;
    bool rejected = false;
    try {
        mode.set(static_cast<TimelineMode>(7));
        sync(f);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.timeline().snapshot(id).order == order, "invalid mode partially published");
    mode.set(TimelineMode::Start);
    sync(f);
    check(f.services.destroy(id), "Timeline teardown failed");
    sync(f);
    check(f.services.components().component_count() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().declaration_order().empty() && blocked_roots(f.services.interactions()).empty(),
          "Timeline teardown leaked resources");
}

void visual_status_and_material() {
    Fixture f;
    Signal<ThemeConfig> config{ThemeConfig{}};
    Signal<TimelineTone> tone{TimelineTone::Red};
    Signal<bool> reverse{false};
    Signal<bool> loading{false};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Timeline(TimelineProps{}.reverse(reverse).layout(LayoutStyle{}.width(dp(320))),
                           {TimelineItem{u8"a", u8"A"}.tone(tone), TimelineItem{u8"b", u8"B"}.loading(loading)});
              }});
    }});
    sync(f);
    const auto owner = f.services.timeline().mounted()[0];
    auto parts = f.services.timeline().snapshot(owner);
    const auto tail = parts.items[0].tail;
    const auto first = bounds(f, parts.items[0].viewport);
    const auto next = bounds(f, parts.items[1].viewport);
    check(std::abs(first.y + tail.y + tail.height - next.y - parts.items[1].dot.y) < .001F,
          "tail did not end at next actual node");
    check(std::ranges::any_of(f.services.surfaces().instances().instances(),
                              [&](const auto& q) {
                                  return q.opacity > 0 && q.bounds == std::array{first.x + tail.x, first.y + tail.y,
                                                                                 tail.width, tail.height};
                              }),
          "actual tail quad missing");
    const auto red = f.services.components().theme_scope(owner)->snapshot().timeline().colors.red;
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& e) {
                                  return e.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         e.material.color == red && e.material.visible;
                              }),
          "typed tone did not reach actual dot material");
    const auto node = f.services.components().root(owner);
    const auto measures = f.nodes.require(node).measure_count;
    auto theme = std::make_shared<ThemeConfig>();
    theme->timeline.tokens.red = Color::rgba8(12, 34, 56);
    config.set(*theme);
    sync(f);
    check(f.nodes.require(node).measure_count == measures &&
              std::ranges::any_of(
                  f.services.rounded_effects().packed_instances(),
                  [&](const auto& e) { return e.material.color == *theme->timeline.tokens.red && e.material.visible; }),
          "color Theme update dirtied layout or missed dot");
    for (auto value : {TimelineTone::Default, TimelineTone::Blue, TimelineTone::Green, TimelineTone::Gray}) {
        tone.set(value);
        sync(f);
    }
    loading.set(true);
    reverse.set(true);
    sync(f);
    parts = f.services.timeline().snapshot(owner);
    check(parts.items[0].tail.height == 0 && parts.items[1].tail.height > 0 && parts.items[1].loading,
          "reversed loading tail endpoints drifted");
    check(f.scene.glyph_scene().instances().size() > 0, "static loading has no glyph scene");
}

void empty_flow() {
    Fixture f;
    Signal<bool> visible{false};
    Signal<bool> root{true};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(11)), FlexContent{[&] {
                 Button(ButtonProps{}.wave(false), [] { Text(u8"Before"); });
                 Timeline(TimelineProps{}, {});
                 Timeline(TimelineProps{}.visible(root), {TimelineItem{u8"event", u8"Event"}.visible(visible)});
                 Button(ButtonProps{}.wave(false), [] { Text(u8"After"); });
             }});
    }});
    sync(f);
    const auto before = bounds(f, f.buttons.mounted_buttons()[0].component);
    const auto after = f.buttons.mounted_buttons()[1].component;
    check(bounds(f, after).y == before.y + before.height + 11, "empty/full-hidden Timeline left flow gap");
    const auto id = f.services.timeline().mounted()[1];
    const auto item = f.services.timeline().snapshot(id).items[0].viewport;
    visible.set(true);
    sync(f);
    check(bounds(f, after).y > before.y + before.height + 11, "Timeline did not restore flow");
    root.set(false);
    sync(f);
    check(bounds(f, after).y == before.y + before.height + 11, "root visibility left flow gap");
    root.set(true);
    sync(f);
    check(f.services.timeline().snapshot(id).items[0].viewport == item, "flow recovery remounted item");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        retained_layout();
        visual_status_and_material();
        empty_flow();
        std::cout << "Timeline component contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
