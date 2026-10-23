#include "component/select_component.hpp"
#include "component/tabs_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
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
    const auto& node = f.nodes.require(f.services.components().root(id));
    auto result = node.bounds;
    result.x += node.translation.x;
    result.y += node.translation.y;
    return result;
}

InteractionId interaction(Fixture& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(hit).component == id) {
            return hit;
        }
    }
    throw std::runtime_error("component has no interaction");
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    f.synchronize();
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    f.synchronize();
}

std::size_t tab_stops(Fixture& f) {
    return std::ranges::count_if(f.services.interactions().declaration_order(), [&](auto hit) {
        const auto& row = f.services.interactions().require(hit);
        return row.eligible && row.focusable && row.tab_stop && f.services.interactions().branch_active(hit);
    });
}

void retained_panels_and_navigation() {
    Signal<bool> disabled{false};
    Signal<String> label{String{u8"Draft"}};
    int root_runs{}, first_runs{}, second_runs{};
    std::vector<String> changes, clicks;
    Fixture f;
    f.services.mount(Content{[&] {
        ++root_runs;
        Tabs(TabsProps{}.onChange([&](String key) { changes.push_back(key); }).onTabClick([&](String key) {
            clicks.push_back(key);
        }),
             {TabItem{u8"a", u8"Draft", TabContent{[&] {
                          ++first_runs;
                          Flex(FlexProps{}.vertical(true), FlexContent{[] {
                                   Input(InputProps{}.defaultValue(u8"Original"));
                                   Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
                               }});
                      }}}
                  .label(label)
                  .disabled(disabled),
              TabItem{u8"disabled", u8"Locked", TabContent{[] { Text(u8"Locked panel"); }}}.disabled(),
              TabItem{u8"b", u8"Details", TabContent{[&] {
                          ++second_runs;
                          Input(InputProps{}.autoFocus().defaultValue(u8"Second"));
                      }}}});
        Button(ButtonProps{}, [] { Text(u8"Outside"); });
    }});
    f.synchronize();
    const auto id = f.services.tabs().mounted()[0];
    const auto original = f.services.tabs().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto second = f.inputs.mounted_inputs()[2];
    const auto select = f.services.select().mounted()[0];
    check(original.active == 0 && f.platform.starts == 0 && tab_stops(f) == 4,
          "initial Tabs selection, inactive autofocus or Tab order failed");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "draft focus failed");
    static_cast<void>(f.inputs.editors().require(input.editor).select_all());
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"保留的草稿"}, f.inputs.sessions().active()})),
          "draft edit failed");
    f.synchronize();
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "nested Select failed to open");
    const auto count = f.services.components().component_count();
    const auto texts = f.scene.size();
    click(f, original.headers[2]);
    check(f.services.tabs().snapshot(id).active == 2 && !f.services.select().snapshot(select).visible &&
              !f.inputs.sessions().active().valid() && tab_stops(f) == 3 &&
              !f.services.components().branch_active(original.panels[0]) &&
              f.services.components().branch_active(original.panels[2]),
          "switch retained old popup, focus session or branch");
    key(f, Key::tab);
    check(f.services.focus().state().focused == second.interaction, "Tab did not enter current panel");
    key(f, Key::tab, KeyModifier::shift);
    check(f.services.focus().state().focused == interaction(f, original.headers[2]), "Shift Tab missed header");
    key(f, Key::left);
    check(f.services.tabs().snapshot(id).active == 0 &&
              f.inputs.editors().require(input.editor).value() == String{u8"保留的草稿"}.bytes() &&
              !f.inputs.sessions().active().valid(),
          "keyboard did not skip disabled or restore retained draft");
    key(f, Key::left);
    check(f.services.tabs().snapshot(id).active == 2, "left did not wrap");
    key(f, Key::home);
    key(f, Key::end);
    const auto before = changes.size();
    key(f, Key::enter);
    key(f, Key::space);
    check(changes.size() == before && clicks.size() == before + 2, "same-key activation callback contract failed");
    key(f, Key::home);
    disabled.set(true);
    f.synchronize();
    check(f.services.tabs().snapshot(id).active == 0 && !f.services.focus().state().focused && tab_stops(f) == 4,
          "disabling selected header changed pane or failed to clear focus");
    label.set(String{u8"Updated / 已更新"});
    f.synchronize();
    check(root_runs == 1 && first_runs == 1 && second_runs == 1 &&
              f.services.tabs().snapshot(id).headers == original.headers &&
              f.services.tabs().snapshot(id).panels == original.panels &&
              f.services.components().component_count() == count && f.scene.size() == texts &&
              f.inputs.mounted_inputs()[0].editor == input.editor,
          "Tabs update rebuilt panels or labels");
    f.services.set_window_active(false);
    f.synchronize();
    static_cast<void>(f.frames.consume_request());
    const auto effects = f.services.rounded_effects().diagnostics();
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.rounded_effects().diagnostics().geometry_updates == effects.geometry_updates &&
              f.services.rounded_effects().diagnostics().material_updates == effects.material_updates,
          "idle Tabs requested a frame or changed effects");
    check(f.services.destroy(id), "Tabs destroy failed");
    f.synchronize();
    check(f.services.tabs().mounted().empty() && f.services.select().mounted().empty() &&
              f.inputs.mounted_inputs().empty() && f.services.interactions().size() == 1,
          "Tabs leaked nested resources");
}

void controlled_keys() {
    Signal<String> active{String{u8"a"}};
    bool echo{};
    std::vector<String> changes;
    Fixture f;
    f.services.mount(Content{[&] {
        Tabs(TabsProps{}.activeKey(active).onChange([&](String key) {
            changes.push_back(key);
            if (echo) {
                active.set(key);
            }
        }),
             {TabItem{u8"a", u8"A", TabContent{[] { Input(InputProps{}); }}},
              TabItem{u8"b", u8"B", TabContent{[] { Input(InputProps{}); }}},
              TabItem{u8"locked", u8"Locked", TabContent{[] { Input(InputProps{}); }}}.disabled()});
    }});
    f.synchronize();
    const auto id = f.services.tabs().mounted()[0];
    const auto s = f.services.tabs().snapshot(id);
    click(f, s.headers[1]);
    check(f.services.tabs().snapshot(id).active == 0 && changes == std::vector<String>{String{u8"b"}} &&
              f.services.focus().state().focused == interaction(f, s.headers[1]),
          "controlled refusal committed selection or lost header focus");
    active.set(String{u8"unknown"});
    f.synchronize();
    check(!f.services.tabs().snapshot(id).active && f.services.tabs().snapshot(id).key == String{u8"unknown"} &&
              !f.services.components().branch_active(s.panels[0]) && tab_stops(f) == 1,
          "unknown controlled key did not retain authority");
    active.set(String{u8"locked"});
    f.synchronize();
    check(f.services.tabs().snapshot(id).active == 2 && f.services.components().branch_active(s.panels[2]) &&
              !f.services.interactions().require(interaction(f, s.headers[2])).eligible,
          "explicit disabled key did not display retained panel");
    echo = true;
    key(f, Key::home);
    check(f.services.tabs().snapshot(id).active == 0 && changes.back() == String{u8"a"}, "controlled echo failed");
}

void overflow_and_locality(float scale) {
    Signal<LogicalLength> width{dp(160)};
    Fixture f;
    f.font_scale = scale;
    f.services.mount(Content{[&] {
        std::vector<TabItem> items;
        for (int i = 0; i < 8; ++i) {
            auto key = String::from_utf8(std::to_string(i)).value();
            items.emplace_back(key, String{u8"Long label / 标签"}, TabContent{[] { Text(u8"Body"); }});
        }
        Tabs(TabsProps{}.layout(LayoutStyle{}.width(width)), items);
    }});
    f.synchronize();
    const auto id = f.services.tabs().mounted()[0];
    auto s = f.services.tabs().snapshot(id);
    check(s.overflow && s.scroll == 0 && tab_stops(f) == 1 &&
              !f.services.interactions().require(interaction(f, s.next)).focusable,
          "overflow navigation became a Tab stop");
    click(f, s.next);
    check(f.services.tabs().snapshot(id).scroll > 0 && f.services.tabs().snapshot(id).active == 0,
          "next arrow failed or changed selection");
    s = f.services.tabs().snapshot(id);
    check(f.inputs.dispatch(ScrollInputEvent{100, 0, s.viewport.x + 5, s.viewport.y + 5}),
          "header wheel was not handled");
    f.synchronize();
    s = f.services.tabs().snapshot(id);
    check(s.scroll == s.maximum_scroll &&
              !f.inputs.dispatch(ScrollInputEvent{1, 0, s.viewport.x + 5, s.viewport.y + s.viewport.height + 20}),
          "wheel escaped header or failed to clamp");
    check(f.services.focus().request_focus(interaction(f, s.headers[0]), FocusModality::keyboard),
          "offscreen focus failed");
    f.synchronize();
    check(f.services.tabs().snapshot(id).scroll == 0, "focus failed to reveal offscreen entry");
    key(f, Key::end);
    s = f.services.tabs().snapshot(id);
    const auto last = bounds(f, s.headers.back());
    check(s.active == 7 && last.x >= s.viewport.x && last.x + last.width <= s.viewport.x + s.viewport.width,
          "End did not reveal last header");
    const auto hit = f.services.hit_test().hit_test({bounds(f, s.next).x + 2, bounds(f, s.next).y + 2});
    check(hit == interaction(f, s.next), "overflow header hit escaped viewport");
    key(f, Key::home);
    const auto first = bounds(f, s.headers[0]);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, first.x + 2, first.y + 2});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 319, 239});
    f.synchronize();
    check(f.services.tabs().snapshot(id).active == 0, "release outside activated a tab");
    // Realize all header glyphs, then scrolling must only patch translation/clip.
    key(f, Key::end);
    key(f, Key::home);

    struct Counts {
        detail::TextSceneId scene;
        std::uint64_t shapes, instances, geometry;
    };

    std::vector<Counts> counts;
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto& record = f.scene.record_counters(text.scene);
        if (f.scene.primitive(text.scene).instances.count > 0) {
            counts.push_back({text.scene, f.scene.text_state(text.scene).counters().shape_count,
                              record.instance_rebuilds, record.geometry_rebuilds});
        }
    }
    click(f, s.next);
    f.synchronize();
    for (const auto& old : counts) {
        const auto& record = f.scene.record_counters(old.scene);
        check(f.scene.text_state(old.scene).counters().shape_count == old.shapes &&
                  record.instance_rebuilds == old.instances && record.geometry_rebuilds == old.geometry,
              "header scroll reshaped or rebuilt existing glyph geometry");
    }
    runtime::NodePropertyWriter writer{f.nodes, f.dirty};
    writer.set_translation(f.services.components().root(id), {17.25F, 13.5F});
    f.synchronize();
    s = f.services.tabs().snapshot(id);
    const auto& moved = f.nodes.require(f.services.components().root(s.headers[0]));
    check(std::abs(moved.translation.x - (17.25F - s.scroll)) < 0.001F && moved.translation.y == 13.5F &&
              bounds(f, s.next).x == 17.25F + 160 - 24,
          "Tabs scroll lost ancestor translation");
    width.set(dp(2400));
    f.synchronize(2400, {0, 0, 2400, 240});
    check(!f.services.tabs().snapshot(id).overflow && f.services.tabs().snapshot(id).scroll == 0,
          "resize did not remove overflow");
    width.set(dp(0));
    f.synchronize();
    check(bounds(f, id).width == 0, "zero-width Tabs failed");
}

void variants_and_theme() {
    Signal<TabsType> type{TabsType::Line};
    Signal<TabsPlacement> placement{TabsPlacement::Top};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<bool> centered{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Tabs(TabsProps{}.type(type).placement(placement).size(size).centered(centered),
                       {TabItem{u8"a", u8"A", TabContent{[&] {
                                    ++runs;
                                    Text(u8"Body");
                                }}},
                        TabItem{u8"b", u8"B", TabContent{[] { Text(u8"Second"); }}}});
              }});
    }});
    f.synchronize();
    const auto id = f.services.tabs().mounted()[0];
    const auto original = f.services.tabs().snapshot(id);
    const auto initial_x = bounds(f, original.headers[0]).x;
    centered.set(true);
    f.synchronize();
    check(bounds(f, original.headers[0]).x > initial_x, "centered header did not center");
    for (auto style : {TabsType::Line, TabsType::Card}) {
        type.set(style);
        for (auto position : {TabsPlacement::Top, TabsPlacement::Bottom}) {
            placement.set(position);
            for (auto value : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
                size.set(value);
                f.synchronize();
                const auto header = bounds(f, original.headers[0]);
                const auto panel = bounds(f, original.panels[0]);
                const float height = style == TabsType::Card ? (value == ControlSize::Small   ? 32.0F
                                                                : value == ControlSize::Large ? 48.0F
                                                                                              : 40.0F)
                                                             : (value == ControlSize::Small   ? 38.0F
                                                                : value == ControlSize::Large ? 56.0F
                                                                                              : 46.0F);
                check(header.height == height &&
                          (position == TabsPlacement::Top ? panel.y > header.y : panel.y < header.y),
                      "Tabs size/type/placement layout failed");
            }
        }
    }
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    ThemeConfig custom;
    custom.tabs.tokens.item_selected = Color::rgba8(160, 30, 50);
    theme.set(custom);
    f.synchronize();
    check(f.nodes.require(node).measure_count == measures && runs == 1, "Tabs color update remeasured or reran panel");
    custom.tabs.tokens.focus_width = dp(0);
    custom.tabs.tokens.border_width = dp(0);
    custom.tabs.tokens.font_size_large = dp(18);
    theme.set(custom);
    f.synchronize();
    check(runs == 1 && f.services.tabs().snapshot(id).headers == original.headers, "Tabs token update remounted");
}

void callback_disposal_and_invalid_mounts() {
    for (int mode = 0; mode < 3; ++mode) {
        runtime::ComponentId owner;
        bool retire{};
        Fixture f;
        f.services.mount(Content{[&] {
            Tabs(TabsProps{}
                     .onChange([&](String) {
                         if (mode == 0) {
                             static_cast<void>(f.services.destroy(owner));
                         }
                     })
                     .onTabClick([&](String) {
                         if (mode == 1) {
                             static_cast<void>(f.services.destroy(owner));
                         }
                     }),
                 {TabItem{u8"a", u8"A", TabContent{[&] {
                              Input(InputProps{}.onBlur([&] {
                                  if (retire) {
                                      static_cast<void>(f.services.destroy(owner));
                                  }
                              }));
                          }}},
                  TabItem{u8"b", u8"B", TabContent{[] { Text(u8"Body"); }}}});
        }});
        f.synchronize();
        owner = f.services.tabs().mounted()[0];
        const auto s = f.services.tabs().snapshot(owner);
        if (mode == 2) {
            static_cast<void>(
                f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard));
            retire = true;
        }
        click(f, s.headers[1]);
        check(f.services.tabs().mounted().empty() && f.inputs.mounted_inputs().empty() &&
                  f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() &&
                  f.services.rounded_effects().live_count() == 0,
              "Tabs callback disposal retained resources");
    }
    for (int mode = 0; mode < 6; ++mode) {
        int runs{};
        Fixture f;
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                TabsProps props;
                if (mode == 0) {
                    props.size(static_cast<ControlSize>(99));
                }
                if (mode == 1) {
                    props.type(static_cast<TabsType>(99));
                }
                if (mode == 2) {
                    props.placement(static_cast<TabsPlacement>(99));
                }
                if (mode == 3) {
                    props.activeKey(u8"a").defaultActiveKey(u8"a");
                }
                Tabs(props, {TabItem{mode == 4 ? String{} : String{u8"a"}, u8"A", TabContent{[&] { ++runs; }}},
                             TabItem{mode == 5 ? String{u8"a"} : String{u8"b"}, u8"B", TabContent{[] {}}}});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && runs == 0 && f.nodes.size() == 0 && f.services.tabs().mounted().empty() &&
                  f.services.interactions().size() == 0,
              "invalid Tabs partially mounted");
    }
    Fixture f;
    f.services.mount(Content{[] {
        Tabs(TabsProps{}, {});
        Tabs(TabsProps{}, {TabItem{u8"a", u8"A", TabContent{[] { Text(u8"Hidden"); }}}.disabled()});
    }});
    f.synchronize();
    check(!f.services.tabs().snapshot(f.services.tabs().mounted()[0]).active &&
              !f.services.tabs().snapshot(f.services.tabs().mounted()[1]).active && tab_stops(f) == 0,
          "empty/all-disabled Tabs selected a panel");
}

void capture_disposal() {
    Signal<bool> disabled{false};
    Fixture f;
    f.services.mount(Content{[&] {
        Tabs(TabsProps{}, {TabItem{u8"a", u8"A", TabContent{[] { Text(u8"First"); }}},
                           TabItem{u8"b", u8"B", TabContent{[] { Text(u8"Second"); }}}.disabled(disabled)});
    }});
    f.synchronize();
    const auto id = f.services.tabs().mounted()[0];
    const auto header = f.services.tabs().snapshot(id).headers[1];
    const auto rect = bounds(f, header);
    const auto press = [&] {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, rect.x + 2, rect.y + 2});
    };
    press();
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == interaction(f, header),
          "Tabs header did not capture pointer");
    disabled.set(true);
    f.synchronize();
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, rect.x + 2, rect.y + 2});
    f.synchronize();
    check(f.services.tabs().snapshot(id).active == 0 && !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "disabled captured header activated or retained capture");
    disabled.set(false);
    f.synchronize();
    press();
    check(f.services.destroy(id), "captured Tabs disposal failed");
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && f.services.interactions().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "captured Tabs leaked pointer or effects");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        retained_panels_and_navigation();
        controlled_keys();
        for (float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
            overflow_and_locality(scale);
        }
        variants_and_theme();
        callback_disposal_and_invalid_mounts();
        capture_disposal();
        std::cout << "Tabs component tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
