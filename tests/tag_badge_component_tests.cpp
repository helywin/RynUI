#include "component/tag_badge_component.hpp"
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

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    f.synchronize();
}

void keyboard(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
    f.synchronize();
}

void tag_control_and_retained_content() {
    Fixture f;
    Signal<bool> checked{false};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    Signal<bool> closable{true};
    Signal<bool> bordered{true};
    Signal<TagColor> color{TagColor{PresetColor::Blue}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs{};
    int icons{};
    int closes{};
    int changes{};
    bool next{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Tag(TagProps{}
                          .color(color)
                          .visible(visible)
                          .closable(closable)
                          .bordered(bordered)
                          .disabled(disabled)
                          .onClose([&] { ++closes; }),
                      TagContent{[&] {
                          ++runs;
                          Text(u8"Device / 设备");
                      }},
                      TagSlots{.icon = TagIcon{[&] {
                                   ++icons;
                                   Icon(IconProps{}.name(IconName::CheckOutlined));
                               }}});
                  CheckableTag(CheckableTagProps{}.checked(checked).disabled(disabled).onChange([&](bool value) {
                      ++changes;
                      next = value;
                  }),
                               TagContent{[] { Text(u8"Filter"); }});
                  Tag(TagProps{}, TagContent{[] { Text(u8"Static"); }});
              }});
    }});
    f.synchronize();
    const auto tag = f.services.tag_badge().tags()[0];
    const auto filter = f.services.tag_badge().tags()[1];
    const auto initial = f.services.tag_badge().tag_snapshot(tag);
    const auto filter_hit = f.services.tag_badge().tag_snapshot(filter).interaction;
    const auto components = f.services.components().component_count();
    const auto scene = f.scene.size();
    check(bounds(f, tag).height == 22 && bounds(f, initial.content).x > bounds(f, initial.icon).x,
          "Tag dimensions/icon order differ");
    check(std::ranges::count_if(f.services.interactions().declaration_order(),
                                [&](auto hit) {
                                    const auto& r = f.services.interactions().require(hit);
                                    return r.eligible && r.tab_stop && f.services.interactions().branch_active(hit);
                                }) == 2,
          "static Tag added focus stop");
    click(f, initial.close);
    check(closes == 1 && f.services.tag_badge().tag_snapshot(tag).visible, "Tag close was not controlled");
    click(f, filter);
    check(changes == 1 && next && !f.services.tag_badge().tag_snapshot(filter).checked,
          "CheckableTag mutated controlled checked");
    checked.set(true);
    f.synchronize();
    check(f.services.tag_badge().tag_snapshot(filter).checked, "checked write-back absent");
    check(f.services.focus().request_focus(filter_hit, FocusModality::keyboard), "checkable focus failed");
    keyboard(f, Key::space);
    check(changes == 2 && !next, "checkable Space activation absent");
    keyboard(f, Key::enter);
    check(changes == 3 && !next, "checkable Enter activation absent");
    const auto measure = f.nodes.require(f.services.components().root(tag)).measure_count;
    const auto content_measure = f.nodes.require(f.services.components().root(initial.content)).measure_count;
    color.set(TagColor{Status::Success});
    bordered.set(false);
    f.synchronize();
    auto custom = ThemeConfig{};
    custom.tag.tokens.default_color = Color::rgba8(10, 20, 30);
    custom.tag.tokens.close_color = Color::rgba8(30, 20, 10);
    theme.set(custom);
    f.synchronize();
    check(f.nodes.require(f.services.components().root(tag)).measure_count == measure &&
              f.nodes.require(f.services.components().root(initial.content)).measure_count == content_measure,
          "Tag material changes remeasured");
    disabled.set(true);
    f.synchronize();
    check(!f.services.focus().state().focused, "disabled Tag retained focus");
    const auto filter_bounds = bounds(f, filter);
    check(f.services.hit_test().hit_test(
              {filter_bounds.x + filter_bounds.width / 2, filter_bounds.y + filter_bounds.height / 2}) != filter_hit,
          "disabled Tag retained hit geometry");
    click(f, initial.close);
    click(f, filter);
    check(closes == 1 && changes == 3, "disabled Tag activated");
    disabled.set(false);
    f.synchronize();
    check(f.services.focus().request_focus(initial.interaction, FocusModality::keyboard), "close focus failed");
    visible.set(false);
    f.synchronize();
    check(!f.services.focus().state().focused && !f.services.components().branch_active(tag),
          "hidden Tag retained focus/activity");
    visible.set(true);
    closable.set(false);
    f.synchronize();
    check(!f.services.interactions().require(initial.interaction).eligible &&
              f.services.tag_badge().tag_snapshot(tag).content == initial.content && runs == 1 && icons == 1,
          "Tag toggle reran slots");
    color.set(TagColor{Color::rgba8(90, 40, 20)});
    custom.tag.tokens.padding_inline = dp(14);
    theme.set(custom);
    f.synchronize();
    check(f.services.tag_badge().tag_snapshot(tag).palette.background == Color::rgba8(90, 40, 20) &&
              f.services.components().component_count() == components && f.scene.size() == scene,
          "Tag identity/custom color changed");
    closable.set(true);
    f.synchronize();
    check(f.services.focus().request_focus(initial.interaction, FocusModality::keyboard),
          "close keyboard focus failed");
    keyboard(f, Key::enter);
    check(closes == 2, "close Enter failed");
    (void)f.frames.consume_request();
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "idle status component requested frames");
    check(f.services.destroy(tag) && f.services.destroy(filter), "Tag destroy failed");
    f.synchronize();
    check(f.services.tag_badge().tags().size() == 1 && !f.services.interactions().contains(initial.interaction),
          "Tag cleanup retained resources");
}

void badge_counts_offsets_and_child_interaction() {
    Fixture f;
    Signal<int> count{5};
    Signal<int> overflow{99};
    Signal<bool> zero{false};
    Signal<bool> dot{false};
    Signal<Status> status{Status::None};
    Signal<LogicalOffset> offset{LogicalOffset{}};
    Signal<String> label{String{}};
    Signal<BadgeSize> size{BadgeSize::Default};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs{};
    int clicks{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Badge(BadgeProps{}
                            .count(count)
                            .overflowCount(overflow)
                            .showZero(zero)
                            .dot(dot)
                            .status(status)
                            .offset(offset)
                            .size(size),
                        BadgeContent{[&] {
                            ++runs;
                            Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Inbox"); });
                        }});
                  Badge(BadgeProps{}.count(count).showZero(zero));
                  Badge(BadgeProps{}.status(Status::Processing).text(label));
                  Badge(BadgeProps{}.dot());
              }});
    }});
    f.synchronize();
    const auto id = f.services.tag_badge().badges()[0];
    const auto standalone = f.services.tag_badge().badges()[1];
    const auto status_id = f.services.tag_badge().badges()[2];
    const auto initial = f.services.tag_badge().badge_snapshot(id);
    const auto child = bounds(f, initial.content);
    const auto initial_count = f.services.components().component_count();
    check(initial.display_count == String{u8"5"} && initial.indicator_bounds.width == 20 &&
              bounds(f, id).width == child.width && bounds(f, id).height == child.height,
          "wrapped Badge changed child layout");
    check(initial.indicator_bounds.x == child.x + child.width - 10 && initial.indicator_bounds.y == child.y - 10,
          "Badge corner placement wrong");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(clicks == 1, "Badge swallowed child pointer");
    const auto hit = f.buttons.mounted_buttons()[0].interaction;
    check(f.services.hit_test().hit_test({child.x + child.width - 1, child.y + 1}) == hit,
          "Badge indicator stole the covered child hit");
    check(f.services.focus().request_focus(hit, FocusModality::keyboard), "Badge child focus failed");
    keyboard(f, Key::enter);
    check(clicks == 2, "Badge swallowed child keyboard");
    count.set(100);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"99+"} &&
              f.services.tag_badge().badge_snapshot(id).indicator_bounds.width > 20,
          "overflow pill failed");
    overflow.set(9);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"9+"}, "reactive overflow failed");
    offset.set(LogicalOffset{4, 8});
    f.synchronize();
    auto shifted = f.services.tag_badge().badge_snapshot(id);
    check(shifted.indicator_bounds.x == child.x + child.width - shifted.indicator_bounds.width / 2 + 4 &&
              shifted.indicator_bounds.y == child.y - shifted.indicator_bounds.height / 2 + 8,
          "Badge logical offset differs");
    count.set(0);
    f.synchronize();
    check(!f.services.tag_badge().badge_snapshot(id).shown && bounds(f, standalone).width == 0 &&
              bounds(f, id).width == child.width,
          "zero Badge visibility/layout failed");
    zero.set(true);
    size.set(BadgeSize::Small);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"0"} &&
              f.services.tag_badge().badge_snapshot(id).indicator_bounds.height == 14,
          "showZero/small failed");
    dot.set(true);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).dot &&
              f.services.tag_badge().badge_snapshot(id).display_count.empty() &&
              f.services.tag_badge().badge_snapshot(id).indicator_bounds.width == 6,
          "dot Badge failed");
    zero.set(false);
    status.set(Status::Success);
    label.set(String{u8"Running / 运行中"});
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).status &&
              f.services.tag_badge().badge_snapshot(id).color == resolve_theme().map().color_success &&
              bounds(f, status_id).width > 6,
          "status/text Badge failed");
    const auto status_measure = f.nodes.require(f.services.components().root(id)).measure_count;
    status.set(Status::Warning);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).color == resolve_theme().map().color_warning &&
              f.nodes.require(f.services.components().root(id)).measure_count == status_measure,
          "Badge status color remeasured content");
    count.set(3);
    dot.set(false);
    f.synchronize();
    check(!f.services.tag_badge().badge_snapshot(id).status, "count did not take precedence");
    const auto measure = f.nodes.require(f.services.components().root(id)).measure_count;
    const auto child_measure = f.nodes.require(f.services.components().root(initial.content)).measure_count;
    ThemeConfig config;
    config.badge.tokens.background = Color::rgba8(20, 80, 140);
    config.badge.tokens.count_color = Color::rgba8(250, 250, 250);
    theme.set(config);
    f.synchronize();
    check(f.nodes.require(f.services.components().root(id)).measure_count == measure &&
              f.nodes.require(f.services.components().root(initial.content)).measure_count == child_measure &&
              f.services.tag_badge().badge_snapshot(id).color == *config.badge.tokens.background,
          "Badge color change remeasured");
    config.badge.tokens.indicator_height_small = dp(18);
    theme.set(config);
    f.synchronize();
    check(f.services.tag_badge().badge_snapshot(id).indicator_bounds.height == 18 && runs == 1 &&
              f.services.components().component_count() == initial_count,
          "Badge metrics remounted child");
    const auto surfaces = f.services.surfaces().size();
    check(f.services.destroy(id), "Badge destroy failed");
    f.synchronize();
    check(f.buttons.mounted_buttons().empty() && !f.services.interactions().contains(hit) &&
              f.services.surfaces().size() < surfaces,
          "Badge leaked child/resources");
}

void invalid_props_and_callback_retirement() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(Content{[&] {
                if (mode == 0) {
                    Badge(BadgeProps{}.count(-1));
                } else if (mode == 1) {
                    Badge(BadgeProps{}.overflowCount(-1));
                } else if (mode == 2) {
                    Tag(TagProps{}.color(TagColor{static_cast<PresetColor>(99)}), TagContent{[] {}});
                } else {
                    Badge(BadgeProps{}.status(static_cast<Status>(99)));
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.nodes.size() == 0 && f.services.tag_badge().tags().empty() &&
                  f.services.tag_badge().badges().empty(),
              "invalid status props left partial mount");
    }
    Fixture f;
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        Tag(TagProps{}.closable().onClose([&] { f.services.destroy(owner); }), TagContent{[] { Text(u8"Remove"); }});
    }});
    f.synchronize();
    owner = f.services.tag_badge().tags()[0];
    const auto close = f.services.tag_badge().tag_snapshot(owner).close;
    click(f, close);
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "close callback retirement leaked");
}

void narrow_geometry_and_keyboard_feedback() {
    Fixture f;
    f.services.mount(Content{[] {
        Tag(TagProps{}.closable().layout(LayoutStyle{}.width(dp(0)).height(dp(0))), TagContent{[] { Text(u8"Zero"); }});
        Tag(TagProps{}.closable().layout(LayoutStyle{}.width(dp(2))), TagContent{[] { Text(u8"Narrow"); }});
        CheckableTag(CheckableTagProps{}, TagContent{[] { Text(u8"Press"); }});
        Badge(BadgeProps{}.count(100).layout(LayoutStyle{}.width(dp(2))));
    }});
    f.synchronize();
    const auto id = f.services.tag_badge().tags()[2];
    const auto hit = f.services.tag_badge().tag_snapshot(id).interaction;
    check(f.services.focus().request_focus(hit, FocusModality::keyboard), "Tag feedback focus failed");
    f.services.focus().dispatch({Key::space, KeyAction::down});
    f.synchronize();
    check(f.services.tag_badge().tag_snapshot(id).palette.background == resolve_theme().tag().colors.primary_active,
          "Tag keyboard press did not update material");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& e) {
                                  return e.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         e.material.opacity > 0 &&
                                         e.material.color == resolve_theme().tag().colors.focus;
                              }),
          "Tag keyboard focus ring missing");
    f.services.focus().dispatch({Key::space, KeyAction::up});
    f.synchronize();
    runtime::NodePropertyWriter writer{f.nodes, f.dirty};
    writer.set_translation(f.services.components().root(id), {13, 17});
    f.synchronize();
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& e) { return e.geometry.translation == runtime::Point{13, 17}; }),
          "Tag retained effects lost translation");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        tag_control_and_retained_content();
        badge_counts_offsets_and_child_interaction();
        invalid_props_and_callback_retirement();
        narrow_geometry_and_keyboard_feedback();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
