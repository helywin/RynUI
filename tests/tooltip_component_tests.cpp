#include "component/tooltip_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid Tooltip input accepted");
}

void tick(Fixture& f, std::int64_t micros) {
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(micros));
    f.synchronize();
}

void move(Fixture& f, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, x, y});
}

void click(Fixture& f, runtime::Rect bounds, PointerButton button = PointerButton::primary) {
    const float x = bounds.x + bounds.width / 2;
    const float y = bounds.y + bounds.height / 2;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, button, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, button, x, y});
    f.synchronize();
}

void rich_title_and_actions() {
    Fixture f;
    Signal<String> rich{String{u8"short"}};
    Signal<bool> available{true};
    int root_runs{};
    int title_runs{};
    int trigger_runs{};
    int clicks{};
    std::vector<bool> requests;
    f.services.mount(Content{[&] {
        ++root_runs;
        Tooltip(
            TooltipProps{}.trigger(TooltipTriggerMode::Click).titleAvailable(available).onOpenChange([&](bool value) {
                requests.push_back(value);
            }),
            TooltipTrigger{[&] {
                ++trigger_runs;
                Button(ButtonProps{}.onClick([&] { ++clicks; }), ButtonContent{[] { Text(u8"Click title"); }});
            }},
            TooltipTitle{[&] {
                ++title_runs;
                Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                         Text(TextProps{}.content(rich));
                         Icon(IconProps{}.name(IconName::CheckOutlined));
                     }});
            }});
        Text(u8"sibling");
    }});
    f.synchronize();
    const auto id = f.services.tooltip().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto bounds = f.nodes.require(button.node).bounds;
    const auto popup = f.services.components().children(id)[1];
    const auto popup_children = f.services.components().children(popup);
    const std::vector<runtime::ComponentId> retained(popup_children.begin(), popup_children.end());
    click(f, bounds);
    check(clicks == 1 && f.services.tooltip().snapshot(id).visible && requests == std::vector<bool>{true},
          "click did not preserve Button activation and open rich title");
    const auto width = f.services.tooltip().snapshot(id).bounds.width;
    rich.set(String{u8"Longer reactive rich title 内容"});
    f.synchronize();
    check(f.services.tooltip().snapshot(id).bounds.width > width && root_runs == 1 && trigger_runs == 1 &&
              title_runs == 1,
          "rich title failed to resize independently");
    available.set(false);
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible && !f.services.next_frame_deadline(),
          "unavailable rich title remained visible");
    available.set(true);
    f.synchronize();
    check(f.services.tooltip().snapshot(id).visible, "rich title did not restore retained open");
    click(f, bounds);
    check(clicks == 2 && !f.services.tooltip().snapshot(id).visible && requests == std::vector<bool>({true, false}),
          "click did not toggle closed");
    click(f, bounds);
    const auto popup_bounds = f.services.tooltip().snapshot(id).bounds;
    click(f, popup_bounds);
    check(f.services.tooltip().snapshot(id).visible, "popup click was classified as outside");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, 310, 230});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 310, 230});
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible, "blank window did not dismiss click title");
    check(std::equal(retained.begin(), retained.end(), f.services.components().children(popup).begin()),
          "rich title remounted on close");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, bounds.x + 5, bounds.y + 5});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 310, 230});
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible && clicks == 3, "drag outside was treated as click");
    check(f.services.destroy(id), "rich title destroy failed");
    f.synchronize();
    check(f.services.text().scene_service().size() == 1 && f.services.interactions().size() == 0,
          "rich title leaked retained children");
    Fixture invalid;
    rejects([&] {
        invalid.services.mount(Content{[] {
            Tooltip(TooltipProps{}.title(String{}), TooltipTrigger{[] { Text(u8"trigger"); }},
                    TooltipTitle{[] { Text(u8"title"); }});
        }});
    });
    Fixture trigger_invalid;
    rejects([&] {
        trigger_invalid.services.mount(Content{[] {
            Tooltip(TooltipProps{}.trigger(TooltipTriggerMode::Click).triggers(TooltipTriggers{}),
                    TooltipTrigger{[] { Text(u8"trigger"); }});
        }});
    });
    Fixture rollback;
    try {
        rollback.services.mount(Content{[] {
            Tooltip(TooltipProps{}, TooltipTrigger{[] { Text(u8"trigger"); }}, TooltipTitle{[] {
                        Text(u8"partial title");
                        throw std::runtime_error("title mount failed");
                    }});
        }});
    } catch (const std::runtime_error&) {
    }
    check(rollback.services.tooltip().mounted().empty() && rollback.services.interactions().size() == 0 &&
              rollback.services.surfaces().size() == 0 && rollback.scene.size() == 0,
          "rich title rollback leaked resources");
}

void context_composition_and_controlled_click() {
    Fixture f;
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    Signal<TooltipTriggers> actions{TooltipTriggers{true, true, true, true}};
    std::vector<bool> requests;
    f.services.mount(Content{[&] {
        Tooltip(TooltipProps{}
                    .title(String{u8"actions"})
                    .open(open)
                    .disabled(disabled)
                    .triggers(actions)
                    .onOpenChange([&](bool value) { requests.push_back(value); }),
                TooltipTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Actions"); }}); }});
    }});
    f.synchronize();
    const auto id = f.services.tooltip().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto bounds = f.nodes.require(button.node).bounds;
    click(f, bounds);
    click(f, bounds);
    check(requests == std::vector<bool>({true, false}) && !f.services.tooltip().snapshot(id).visible,
          "controlled click failed to cancel unacknowledged request");
    tick(f, 1000000);
    tick(f, 2000000);
    check(requests.size() == 2, "click dismissal reopened due to hover/focus");
    click(f, bounds, PointerButton::secondary);
    check(requests == std::vector<bool>({true, false, true}), "context menu did not request open");
    open.set(true);
    f.synchronize();
    const auto geometry = f.services.tooltip().snapshot(id);
    check(geometry.visible &&
              geometry.anchor == runtime::Rect{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2, 0, 0},
          "context menu did not anchor at pointer");
    move(f, 310, 230);
    tick(f, 3000000);
    check(f.services.tooltip().snapshot(id).visible && f.services.tooltip().snapshot(id).anchor == geometry.anchor,
          "context pointer latch moved or closed on hover leave");
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    check(requests.back() == false, "Escape did not dismiss context menu");
    open.set(false);
    disabled.set(true);
    const auto count = requests.size();
    click(f, bounds);
    click(f, bounds, PointerButton::secondary);
    check(requests.size() == count, "disabled Tooltip requested an action");
    disabled.set(false);
    click(f, bounds);
    actions.set(TooltipTriggers{});
    f.synchronize();
    check(requests.back() == false, "config update did not clear action latch");
    Fixture destroyed;
    runtime::ComponentId dying;
    destroyed.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.title(String{u8"destroy"}).trigger(TooltipTriggerMode::Click).onOpenChange([&](bool) {
            destroyed.services.destroy(dying);
        }),
                TooltipTrigger{[] { Text(u8"trigger"); }});
    }});
    destroyed.synchronize();
    dying = destroyed.services.tooltip().mounted()[0];
    click(destroyed, destroyed.nodes.require(destroyed.services.components().root(dying)).bounds);
    check(destroyed.services.tooltip().mounted().empty() && destroyed.services.interactions().size() == 0,
          "destructive click callback retained resources");
}

void nested_and_destroyed_trigger_actions() {
    Fixture f;
    int activations{};
    f.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.title(String{u8"outer"}).trigger(TooltipTriggerMode::Click), TooltipTrigger{[&] {
                    Tooltip(TooltipProps{}.title(String{u8"inner"}).trigger(TooltipTriggerMode::Click),
                            TooltipTrigger{[&] {
                                Button(ButtonProps{}.onClick([&] { ++activations; }),
                                       ButtonContent{[] { Text(u8"nested"); }});
                            }});
                }});
    }});
    f.synchronize();
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, f.nodes.require(button.node).bounds);
    check(activations == 1 && f.services.tooltip().mounted().size() == 2, "nested triggers lost child activation");
    for (auto id : f.services.tooltip().mounted()) {
        check(f.services.tooltip().snapshot(id).visible, "nested action observer skipped ancestor");
    }
    click(f, {310, 230, 2, 2});
    for (auto id : f.services.tooltip().mounted()) {
        check(!f.services.tooltip().snapshot(id).visible, "nested outside dismissal skipped a title");
    }
    Fixture removed;
    runtime::ComponentId dying;
    int callbacks{};
    removed.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.title(String{u8"removed"}).trigger(TooltipTriggerMode::Click).onOpenChange([&](bool) {
            ++callbacks;
        }),
                TooltipTrigger{[&] {
                    Button(ButtonProps{}.onClick([&] { removed.services.destroy(dying); }),
                           ButtonContent{[] { Text(u8"destroy in child"); }});
                }});
    }});
    removed.synchronize();
    dying = removed.services.tooltip().mounted()[0];
    click(removed, removed.nodes.require(removed.buttons.mounted_buttons()[0].node).bounds);
    check(callbacks == 0 && removed.services.tooltip().mounted().empty(),
          "child destruction called stale tooltip observer");
}

void geometry_and_api() {
    static_assert(std::is_same_v<decltype(TooltipProps{}.title(String{u8"提示"}).open(true)), TooltipProps&>);
    const runtime::Rect viewport{0, 0, 320, 240};
    for (int value = 0; value < 12; ++value) {
        const auto placement = static_cast<TooltipPlacement>(value);
        const auto s = detail::position_tooltip({100, 80, 40, 20}, {60, 30}, viewport, placement, 12, false);
        const int axis = value / 3;
        check(axis == 0   ? s.bounds.y == 38
              : axis == 1 ? s.bounds.y == 112
              : axis == 2 ? s.bounds.x == 28
                          : s.bounds.x == 152,
              "placement primary axis wrong");
    }
    const auto flip = detail::position_tooltip({2, 0, 20, 20}, {80, 30}, viewport, TooltipPlacement::Top, 12, true);
    check(flip.placement == TooltipPlacement::Bottom && flip.bounds.x == 0 && flip.bounds.y == 32,
          "overflow did not flip and shift");
    const auto tiny =
        detail::position_tooltip({0, 0, 1, 1}, {250, 32}, {0, 0, 1, 1}, TooltipPlacement::Right, 12, true);
    check(tiny.bounds == runtime::Rect{0, 0, 1, 1}, "tiny viewport produced invalid bounds");
    rejects([&] { detail::position_tooltip({NAN, 0, 1, 1}, {}, viewport, TooltipPlacement::Top, 1, true); });
    rejects([&] { detail::position_tooltip({}, {}, viewport, static_cast<TooltipPlacement>(99), 1, true); });
    Fixture invalid;
    rejects([&] {
        invalid.services.mount(Content{
            [] { Tooltip(TooltipProps{}.open(false).defaultOpen(true), TooltipTrigger{[] { Text(u8"invalid"); }}); }});
    });
    check(invalid.services.tooltip().mounted().empty() && invalid.services.interactions().size() == 0,
          "invalid mode acquired resources");
}

void delayed_hover_and_focus() {
    Fixture f;
    int clicks{};
    std::vector<bool> requests;
    f.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.title(String{u8"键盘与鼠标提示"}).onOpenChange([&](bool v) { requests.push_back(v); }),
                TooltipTrigger{[&] {
                    Button(ButtonProps{}.onClick([&] { ++clicks; }), ButtonContent{[] { Text(u8"Trigger"); }});
                }});
        Text(u8"later sibling");
    }});
    f.synchronize();
    const auto id = f.services.tooltip().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto bounds = f.nodes.require(button.node).bounds;
    const auto owner_measure = f.nodes.require(f.services.components().root(id)).measure_count;
    const auto scene_count = f.services.text().scene_service().size();
    move(f, bounds.x + 10, bounds.y + 10);
    tick(f, 1000000);
    check(!f.services.tooltip().snapshot(id).visible &&
              f.services.next_frame_deadline()->count_microseconds() == 1100000,
          "hover did not schedule delay using current frame time");
    move(f, 300, 200);
    tick(f, 1050000);
    check(!f.services.tooltip().snapshot(id).visible, "leaving before delay showed Tooltip");
    tick(f, 1150000);
    check(!f.services.next_frame_deadline(), "cancelled hover left a deadline");
    move(f, bounds.x + 10, bounds.y + 10);
    tick(f, 2000000);
    tick(f, 2100000);
    const auto s = f.services.tooltip().snapshot(id);
    check(s.visible && s.placement == TooltipPlacement::Bottom && s.bounds.y >= bounds.y + bounds.height,
          "visible Tooltip did not flip away from window edge");
    check(f.nodes.require(f.services.components().root(id)).measure_count == owner_measure,
          "opening Tooltip changed trigger layout");
    check(f.services.text().scene_service().size() == scene_count, "Tooltip remounted text on show");
    const auto paint = f.services.components().paint_traversal();
    const auto popup = f.services.components().children(id)[1];
    check(f.services.components().in_window_layer(paint.back().component) &&
              f.services.components().parent(paint.back().component) == popup,
          "Tooltip text was not topmost");
    const auto quad_updates = f.services.surfaces().diagnostics();
    f.synchronize();
    check(f.services.surfaces().diagnostics().geometry_updates == quad_updates.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates == quad_updates.material_updates &&
              !f.services.next_frame_deadline(),
          "visible idle Tooltip kept updating");
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "trigger focus failed");
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible && f.services.focus().state().focused == button.interaction,
          "Escape failed to dismiss or stole child focus");
    tick(f, 2500000);
    check(!f.services.tooltip().snapshot(id).visible, "Escape dismissal reopened while focus stayed");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    check(clicks == 1, "Tooltip swallowed child activation");
    move(f, 300, 200);
    f.services.focus().clear_focus();
    tick(f, 2600000);
    f.services.focus().request_focus(button.interaction, FocusModality::keyboard);
    tick(f, 2700000);
    check(f.services.tooltip().snapshot(id).visible, "keyboard focus did not reopen Tooltip");
    f.services.set_window_active(false);
    check(!f.services.tooltip().snapshot(id).visible && !f.services.next_frame_deadline(),
          "inactive window retained Tooltip");
    check(f.services.destroy(id) && f.services.tooltip().mounted().empty(), "destroy retained Tooltip host state");
    f.synchronize();
    check(f.services.text().scene_service().size() == 1 && f.services.interactions().size() == 0,
          "Tooltip destroy leaked child resources or damaged sibling");
}

void controlled_disabled_and_reentrancy() {
    Fixture f;
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    Signal<String> title{String{u8"Tooltip"}};
    std::vector<bool> requests;
    f.services.mount(Content{[&] {
        Tooltip(TooltipProps{}
                    .title(title)
                    .open(open)
                    .disabled(disabled)
                    .mouseEnterDelay(Duration{})
                    .mouseLeaveDelay(Duration{})
                    .onOpenChange([&](bool value) { requests.push_back(value); }),
                TooltipTrigger{
                    [] { Button(ButtonProps{}.disabled(true), ButtonContent{[] { Text(u8"disabled child"); }}); }});
    }});
    f.synchronize();
    const auto id = f.services.tooltip().mounted()[0];
    const auto bounds = f.nodes.require(f.services.components().root(id)).bounds;
    move(f, bounds.x + 10, bounds.y + 10);
    tick(f, 1000);
    check(requests == std::vector<bool>{true} && !f.services.tooltip().snapshot(id).visible,
          "controlled Tooltip overwrote caller open or disabled child rejected hover");
    tick(f, 2000);
    check(requests.size() == 1, "controlled hover repeated equal requests");
    move(f, 300, 200);
    tick(f, 3000);
    check(requests == std::vector<bool>({true, false}), "leave failed to cancel an unacknowledged controlled request");
    open.set(true);
    f.synchronize();
    check(f.services.tooltip().snapshot(id).visible, "controlled writeback did not show Tooltip");
    disabled.set(true);
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible, "disabled Tooltip remained visible");
    disabled.set(false);
    title.set(String{});
    f.synchronize();
    check(!f.services.tooltip().snapshot(id).visible && !f.services.next_frame_deadline(),
          "empty title retained popup/timer");
    Fixture self;
    runtime::ComponentId dying;
    self.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.title(String{u8"destroy"}).mouseEnterDelay(Duration{}).onOpenChange([&](bool value) {
            if (value) {
                self.services.destroy(dying);
            }
        }),
                TooltipTrigger{[] { Text(u8"trigger"); }});
    }});
    self.synchronize();
    dying = self.services.tooltip().mounted()[0];
    move(self, 3, 3);
    tick(self, 1000);
    check(self.services.tooltip().mounted().empty() && self.services.surfaces().size() == 0 &&
              !self.services.next_frame_deadline(),
          "reentrant destroy leaked Tooltip");
}

void theme_and_geometry_updates() {
    Fixture f;
    ThemeConfig config;
    Signal<ThemeConfig> theme{config};
    Signal<bool> arrow{true};
    int runs{};
    f.services.mount(Content{[&] {
        ++runs;
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Tooltip(TooltipProps{}.title(String{u8"长文字 Long Tooltip"}).open(true).arrow(arrow),
                          TooltipTrigger{[] { Text(u8"anchor"); }});
              }});
    }});
    f.synchronize();
    const auto id = f.services.tooltip().mounted()[0];
    const auto popup = f.services.components().children(id)[1];
    const auto node = f.services.components().root(popup);
    const auto measures = f.nodes.require(node).measure_count;
    const auto before = f.services.surfaces().diagnostics();
    const auto quad_count = [&] {
        std::size_t count{};
        for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
            if (command.kind == graphics::SceneDrawKind::quad) {
                count += command.instance_count;
            }
        }
        return count;
    };
    const auto with_arrow = quad_count();
    config.tooltip.tokens.background = Color::rgba8(200, 0, 0);
    config.tooltip.tokens.text = Color::rgba8(255, 255, 0);
    theme.set(config);
    f.synchronize();
    check(f.nodes.require(node).measure_count == measures && runs == 1 &&
              f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates,
          "Tooltip color update remeasured or remounted geometry");
    arrow.set(false);
    f.synchronize();
    check(quad_count() < with_arrow, "hiding arrow left stale drawn instances");
    config.tooltip.tokens.max_width = dp(60);
    theme.set(config);
    f.synchronize();
    check(f.services.tooltip().snapshot(id).bounds.width <= 60 && f.nodes.require(node).measure_count > measures,
          "Tooltip maxWidth did not remeasure");
    config.typography.tokens.font_weight = 700;
    theme.set(config);
    f.synchronize();
    const auto popup_text = f.services.components().children(popup).front();
    check(f.services.text().resolved_typography(popup_text).font_weight == 700,
          "Tooltip ignored theme font weight update");
    const auto normal = resolve_theme({});
    check(normal.tooltip().max_width == 250 && normal.tooltip().z_index_popup == normal.seed().z_index_popup_base + 70,
          "Ant Tooltip defaults wrong");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).tooltip().background != normal.tooltip().background, "Dark Tooltip missing");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).tooltip().metrics() != normal.tooltip().metrics(), "Compact Tooltip missing");
    const auto parent = resolve_theme(config);
    check(resolve_theme({}, &parent).tooltip() == parent.tooltip(), "Tooltip inheritance missing");
    ThemeConfig component;
    component.tooltip.algorithm = true;
    component.tooltip.seed.control_height = dp(40);
    const auto custom = resolve_theme(component);
    check(custom.tooltip().min_height == 40 && custom.map() == normal.map(), "component algorithm polluted global map");
    check(parent.identity() != normal.identity() && parent.diagnostic_json().find("\"tooltip\"") != std::string::npos,
          "Tooltip missing from snapshot identity/JSON");
    rejects([] {
        ThemeConfig bad;
        bad.tooltip.tokens.max_width = dp(0);
        (void)resolve_theme(bad);
    });
    f.synchronize(1, {0, 0, 1, 1});
    check(f.services.tooltip().snapshot(id).bounds.width <= 1, "tiny width was not clamped");
}
} // namespace

int main() {
    try {
        geometry_and_api();
        delayed_hover_and_focus();
        controlled_disabled_and_reentrancy();
        theme_and_geometry_updates();
        rich_title_and_actions();
        context_composition_and_controlled_click();
        nested_and_destroyed_trigger_actions();
        std::cout << "Tooltip contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
