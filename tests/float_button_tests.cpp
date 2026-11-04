#include "component/float_button_component.hpp"
#include "component/feedback_component.hpp"
#include "component/tooltip_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void pointer(Fixture& f, PointerAction action, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move || action == PointerAction::cancel
                                       ? PointerButton::none
                                       : PointerButton::primary,
                                   x, y});
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + r.width / 2, r.y + r.height / 2);
    pointer(f, PointerAction::up, r.x + r.width / 2, r.y + r.height / 2);
    f.synchronize();
}

runtime::ComponentId group(Fixture& f) {
    for (auto id : f.services.float_button().mounted()) {
        if (f.services.float_button().snapshot(id).menu.valid()) {
            return id;
        }
    }
    throw std::runtime_error("Missing Group");
}

void basic() {
    Fixture f;
    Signal<FloatButtonType> type{FloatButtonType::Default};
    Signal<bool> disabled{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<String> label{String{u8"Help"}};
    int roots{};
    int icons{};
    int clicks{};
    f.services.mount(Content{[&] {
        ++roots;
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  FloatButton(FloatButtonProps{}
                                  .floating(false)
                                  .type(type)
                                  .disabled(disabled)
                                  .shape(FloatButtonShape::Square)
                                  .tooltip(u8"Action")
                                  .badge(BadgeProps{}.count(3))
                                  .onClick([&] { ++clicks; }),
                              {.icon = FloatButtonIcon{[&] {
                                   ++icons;
                                   Icon(IconProps{}.name(IconName::QuestionOutlined));
                               }},
                               .content = FloatButtonContent{[&] { Text(TextProps{}.content(label)); }}});
              }});
    }});
    f.synchronize();
    const auto id = f.services.float_button().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    check(bounds(f, id).width == 40 && bounds(f, id).height == 40, "Default size differs");
    click(f, id);
    check(clicks == 1, "FloatButton click failed");
    type.set(FloatButtonType::Primary);
    ThemeConfig custom;
    custom.float_button.tokens.size = dp(56);
    custom.float_button.tokens.icon_size = dp(22);
    custom.float_button.tokens.primary_background = Color::rgba8(20, 80, 120);
    theme.set(custom);
    pointer(f, PointerAction::move, 319, 239);
    f.synchronize();
    check(bounds(f, id).width == 56 && f.buttons.snapshot(button.component).presentation_background ==
                                           *custom.float_button.tokens.primary_background,
          "Reactive FloatButton theme/type failed");
    check(f.buttons.snapshot(button.component).shape == ButtonShape::Default, "Square lost rounded token corners");
    bool resized_icon{};
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto font = f.services.text().resolved_typography(text.component);
        resized_icon = resized_icon || font.font_size == 22;
    }
    check(resized_icon, "FloatButton icon token did not reach semantic typography");
    check(roots == 1 && icons == 1, "Prop/theme update reran composition");
    label.set(String{u8"Long floating content can wrap on multiple lines"});
    f.synchronize();
    check(bounds(f, id).height > 56, "FloatButton content was clipped to a fixed square");
    bool full_content{};
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.text_state(text.scene).content().bytes() == label.get().bytes()) {
            full_content = bounds(f, text.component).height > 56;
        }
    }
    check(full_content, "FloatButton grew its surface but clipped the retained text");
    disabled.set(true);
    f.synchronize();
    click(f, id);
    check(clicks == 1 && f.buttons.snapshot(button.component).disabled, "Disabled FloatButton activated");
    f.services.set_window_active(false);
    f.synchronize();
    static_cast<void>(f.frames.consume_request());
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "Idle FloatButton requested frames");
    check(f.services.destroy(id), "FloatButton destroy failed");
    f.synchronize();
    check(f.services.float_button().mounted().empty() && f.buttons.mounted_buttons().empty() &&
              f.services.interactions().size() == 0,
          "FloatButton leaked child resources");
}

void positioning_drag() {
    Fixture f;
    Signal<bool> disabled{false};
    Signal<bool> draggable{true};
    int clicks{};
    f.services.mount(Content{[&] {
        FloatButton(FloatButtonProps{}
                        .disabled(disabled)
                        .draggable(draggable)
                        .tooltip(TooltipProps{}.title(String{u8"Drag"}))
                        .onClick([&] { ++clicks; }));
        Text(u8"Document content");
    }});
    f.synchronize();
    const auto id = f.services.float_button().mounted()[0];
    check(bounds(f, id) == runtime::Rect{256, 152, 40, 40}, "Window layer did not use viewport insets");
    auto r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + 20, r.y + 20);
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(),
          "Drag did not retain Button capture");
    pointer(f, PointerAction::move, 1000, 1000);
    f.synchronize();
    check(bounds(f, id).x == 280 && bounds(f, id).y == 200 && f.services.float_button().snapshot(id).dragging,
          "Drag failed viewport clamp");
    pointer(f, PointerAction::up, 300, 220);
    check(clicks == 0 && !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "Drag clicked or retained capture");
    click(f, id);
    check(clicks == 1, "Click after drag remained suppressed");
    r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + 20, r.y + 20);
    disabled.set(true);
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.float_button().snapshot(id).dragging,
          "Disable retained drag/capture");
    disabled.set(false);
    f.synchronize();
    pointer(f, PointerAction::down, r.x + 20, r.y + 20);
    draggable.set(false);
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "Disabling draggable retained capture");
    f.services.destroy(id);
    f.synchronize();
    Fixture g;
    g.services.mount(Content{[] {
        FloatButtonGroup(FloatButtonGroupProps{}.trigger(FloatButtonTrigger::Click).draggable(true),
                         FloatButtonGroupContent{[] {
                             FloatButton();
                             FloatButton();
                         }});
    }});
    g.synchronize();
    const auto grouped = group(g);
    const auto trigger = g.services.float_button().snapshot(grouped).trigger;
    click(g, trigger);
    const auto before = bounds(g, trigger);
    pointer(g, PointerAction::down, before.x + 20, before.y + 20);
    pointer(g, PointerAction::move, before.x - 30, before.y);
    g.synchronize();
    pointer(g, PointerAction::up, before.x - 30, before.y);
    const auto dragged = bounds(g, trigger);
    check(g.services.float_button().snapshot(grouped).open, "Dragging Group activated its trigger");
    click(g, trigger);
    check(!g.services.float_button().snapshot(grouped).open && bounds(g, trigger) == dragged,
          "Closing a dragged Group moved its trigger");
}

void anchors() {
    Fixture f;
    f.services.mount(Content{[] {
        for (auto anchor : {FloatButtonAnchor::TopLeft, FloatButtonAnchor::TopRight, FloatButtonAnchor::BottomLeft,
                            FloatButtonAnchor::BottomRight}) {
            FloatButton(FloatButtonProps{}.anchor(anchor));
        }
    }});
    f.synchronize();
    const auto ids = f.services.float_button().mounted();
    check(bounds(f, ids[0]) == runtime::Rect{24, 48, 40, 40} && bounds(f, ids[1]) == runtime::Rect{256, 48, 40, 40} &&
              bounds(f, ids[2]) == runtime::Rect{24, 152, 40, 40} &&
              bounds(f, ids[3]) == runtime::Rect{256, 152, 40, 40},
          "Anchor geometry differs");
    check(f.services.layout_and_synchronize({30, 30}, {0, 0, 30, 30}), "Narrow layout failed");
    for (auto id : ids) {
        const auto r = bounds(f, id);
        check(r.x >= 0 && r.y >= 0 && r.width <= 30 && r.height <= 30, "Narrow floating geometry escaped viewport");
    }
}

void groups() {
    for (auto placement : {FloatButtonPlacement::Top, FloatButtonPlacement::Bottom, FloatButtonPlacement::Left,
                           FloatButtonPlacement::Right}) {
        Fixture f;
        Signal<bool> disabled{false};
        Signal<FloatButtonShape> shape{FloatButtonShape::Square};
        int requests{};
        f.services.mount(Content{[&] {
            FloatButtonGroup(FloatButtonGroupProps{}
                                 .floating(false)
                                 .trigger(FloatButtonTrigger::Click)
                                 .placement(placement)
                                 .shape(shape)
                                 .disabled(disabled)
                                 .onOpenChange([&](bool) { ++requests; }),
                             FloatButtonGroupContent{[] {
                                 FloatButton();
                                 FloatButton();
                             }});
        }});
        f.synchronize();
        const auto id = group(f);
        auto snapshot = f.services.float_button().snapshot(id);
        check(!snapshot.open && !f.services.components().branch_active(snapshot.menu), "Click menu initially open");
        click(f, snapshot.trigger);
        snapshot = f.services.float_button().snapshot(id);
        check(snapshot.open && requests == 1, "Click menu failed to open");
        const auto menu = bounds(f, snapshot.menu);
        const auto trigger = bounds(f, snapshot.trigger);
        check((placement == FloatButtonPlacement::Top && menu.y + menu.height <= trigger.y) ||
                  (placement == FloatButtonPlacement::Bottom && trigger.y + trigger.height <= menu.y) ||
                  (placement == FloatButtonPlacement::Left && menu.x + menu.width <= trigger.x) ||
                  (placement == FloatButtonPlacement::Right && trigger.x + trigger.width <= menu.x),
              "Group placement differs");
        const auto member = f.buttons.mounted_buttons()[0];
        check(f.services.focus().request_focus(member.interaction, FocusModality::keyboard),
              "Menu member unreachable by keyboard");
        f.services.focus().dispatch({Key::escape, KeyAction::down});
        f.synchronize();
        check(!f.services.float_button().snapshot(id).open &&
                  f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
              "Closing a focused menu did not restore the trigger");
        click(f, snapshot.trigger);
        pointer(f, PointerAction::down, menu.x + 20, menu.y + 20);
        disabled.set(true);
        f.synchronize();
        check(!f.services.float_button().snapshot(id).open && !f.services.focus().state().focused &&
                  !f.services.pointer().state(PointerIdentity::mouse())->capture,
              "Disabled group retained branch input");
        for (const auto& button : f.buttons.mounted_buttons()) {
            check(f.buttons.snapshot(button.component).disabled, "Group disabled visuals not inherited");
        }
        disabled.set(false);
        shape.set(FloatButtonShape::Circle);
        f.synchronize();
        for (const auto& button : f.buttons.mounted_buttons()) {
            check(f.buttons.snapshot(button.component).shape == ButtonShape::Circle, "Group shape not inherited");
        }
        f.services.focus().dispatch({Key::escape, KeyAction::down});
        f.synchronize();
        check(!f.services.float_button().snapshot(id).open, "Escape failed to close menu");
        f.services.destroy(id);
        f.synchronize();
        check(f.services.float_button().mounted().empty() && f.services.interactions().size() == 0,
              "Group destroy leaked resources");
    }
}

void controlled_hover() {
    Fixture f;
    Signal<bool> open{false};
    Signal<FloatButtonTrigger> trigger{FloatButtonTrigger::Hover};
    int requests{};
    bool requested{};
    f.services.mount(Content{[&] {
        FloatButtonGroup(
            FloatButtonGroupProps{}.floating(false).trigger(trigger).open(open).onOpenChange([&](bool value) {
                ++requests;
                requested = value;
            }),
            FloatButtonGroupContent{[] { FloatButton(); }});
    }});
    f.synchronize();
    const auto id = group(f);
    auto view = f.services.float_button().snapshot(id);
    auto r = bounds(f, view.trigger);
    pointer(f, PointerAction::move, r.x + 20, r.y + 20);
    check(requests == 1 && requested && !f.services.float_button().snapshot(id).open, "Controlled hover mutated open");
    pointer(f, PointerAction::move, r.x + 21, r.y + 21);
    check(requests == 1, "Controlled hover repeated its request for every move");
    open.set(true);
    f.synchronize();
    view = f.services.float_button().snapshot(id);
    r = bounds(f, view.menu);
    pointer(f, PointerAction::move, r.x + 20, r.y + 20);
    check(requests == 1, "Moving into open menu closed/reopened it");
    pointer(f, PointerAction::move, 319, 239);
    check(!requested && f.services.float_button().snapshot(id).open, "Controlled close mutated open");
    open.set(false);
    f.synchronize();
    trigger.set(FloatButtonTrigger::Click);
    f.synchronize();
    const auto last = f.buttons.mounted_buttons().back();
    check(f.services.focus().request_focus(last.interaction, FocusModality::keyboard), "Trigger focus failed");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    check(requested && !f.services.float_button().snapshot(id).open, "Keyboard controlled activation failed");
    open.set(true);
    f.synchronize();
    pointer(f, PointerAction::down, 319, 239);
    check(!requested, "Outside press failed to request close");
    trigger.set(FloatButtonTrigger::None);
    f.synchronize();
    view = f.services.float_button().snapshot(id);
    check(view.open && !f.services.components().branch_active(view.trigger), "Persistent group kept trigger");
}

void back_top() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<FloatButtonScrollState> scroll{FloatButtonScrollState{800, 1200, 200}};
    Signal<FloatButtonShape> shape{FloatButtonShape::Circle};
    std::vector<float> offsets;
    f.services.mount(Content{[&] {
        FloatButtonBackTop(FloatButtonBackTopProps{}
                               .button(FloatButtonProps{}.draggable(true).shape(shape))
                               .scroll(scroll)
                               .showProgress(true)
                               .onScrollTo([&](float offset) {
                                   offsets.push_back(offset);
                                   auto state = scroll.get();
                                   state.offset = offset;
                                   scroll.set(state);
                               }));
    }});
    f.synchronize();
    const auto id = f.services.float_button().mounted()[0];
    check(f.services.float_button().snapshot(id).visible && f.services.float_button().snapshot(id).progress == .8F,
          "BackTop source projection differs");
    click(f, id);
    check(f.services.float_button().snapshot(id).animating, "BackTop failed to animate");
    f.services.tick_animations(animation::AnimationTime::microseconds(225000));
    f.synchronize();
    check(!f.services.float_button().snapshot(id).visible && f.services.float_button().snapshot(id).animating &&
              offsets.back() > 0,
          "Threshold prematurely stopped BackTop");
    const auto halfway = offsets.back();
    f.services.tick_animations(animation::AnimationTime::microseconds(100000));
    check(offsets.back() <= halfway, "BackTop clock regression scrolled backwards");
    f.services.tick_animations(animation::AnimationTime::microseconds(450000));
    f.synchronize();
    check(offsets.back() == 0 && !f.services.float_button().snapshot(id).animating, "BackTop did not finish at zero");
    f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    f.synchronize();
    static_cast<void>(f.frames.consume_request());
    f.synchronize();
    check(!f.services.next_frame_deadline() && !f.frames.pending(), "Completed BackTop kept requesting frames");
    shape.set(FloatButtonShape::Square);
    scroll.set(FloatButtonScrollState{600, 1200, 200});
    f.synchronize();
    bool square_ring{};
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        square_ring = square_ring || (effect.geometry.kind == graphics::RoundedEffectKind::dashed_outline &&
                                      effect.geometry.shape.radius == 8);
    }
    check(square_ring, "Square BackTop retained a circular progress overlay");
    const auto r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + 20, r.y + 20);
    pointer(f, PointerAction::move, r.x + 30, r.y + 30);
    scroll.set(FloatButtonScrollState{50, 1200, 200});
    f.synchronize();
    check(!f.services.float_button().snapshot(id).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "BackTop visibility change retained a pointer gesture");
    f.services.destroy(id);
    f.synchronize();
    check(f.services.rounded_effects().live_count() == 0, "BackTop ring leaked");
}

void reduced_lifecycle() {
    Fixture f;
    Signal<FloatButtonScrollState> scroll{FloatButtonScrollState{600, 1000, 200}};
    int callbacks{};
    runtime::ComponentId id;
    f.services.mount(Content{[&] {
        FloatButtonBackTop(FloatButtonBackTopProps{}.scroll(scroll).onScrollTo([&](float offset) {
            ++callbacks;
            check(offset == 0, "Reduced motion did not jump to zero");
            f.services.destroy(id);
        }));
    }});
    f.synchronize();
    id = f.services.float_button().mounted()[0];
    click(f, id);
    check(callbacks == 1 && f.services.float_button().mounted().empty(), "Destroy during BackTop callback failed");
    f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    check(callbacks == 1 && !f.services.next_frame_deadline(), "Destroyed target retained callback");
    Fixture grouped;
    runtime::ComponentId group_id;
    grouped.services.mount(Content{[&] {
        FloatButtonGroup(FloatButtonGroupProps{}.trigger(FloatButtonTrigger::Click).onOpenChange([&](bool) {
            grouped.services.destroy(group_id);
        }),
                         FloatButtonGroupContent{[] { FloatButton(); }});
    }});
    grouped.synchronize();
    group_id = group(grouped);
    click(grouped, grouped.services.float_button().snapshot(group_id).trigger);
    check(grouped.services.float_button().mounted().empty() && grouped.services.interactions().size() == 0,
          "Destruction during Group callback retained resources");
}

void off_page() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    int callbacks{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}, FlexContent{[&] {
                 FloatButtonBackTop(
                     FloatButtonBackTopProps{}.scroll(FloatButtonScrollState{800, 1200, 200}).onScrollTo([&](float) {
                         ++callbacks;
                     }));
             }});
    }});
    f.synchronize();
    const auto root = f.services.components().root_components()[0];
    const auto id = f.services.float_button().mounted()[0];
    click(f, id);
    f.services.components().set_branch_active(root, false);
    f.services.mark_scene_structure_dirty();
    f.synchronize();
    check(!f.services.float_button().snapshot(id).animating, "Off-page BackTop retained animation");
    f.services.tick_animations(animation::AnimationTime::microseconds(500000));
    check(callbacks == 0, "Hidden page emitted scroll callbacks");
    f.services.destroy(root);
    f.synchronize();
    check(f.services.float_button().mounted().empty(), "Page teardown retained FloatButton");
}

void validation() {
    for (int mode = 0; mode < 6; ++mode) {
        Fixture f;
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                if (mode == 0) {
                    FloatButton(FloatButtonProps{}.shape(static_cast<FloatButtonShape>(99)));
                }
                if (mode == 1) {
                    FloatButtonGroup(FloatButtonGroupProps{}.open(true).defaultOpen(false),
                                     FloatButtonGroupContent{[] {}});
                }
                if (mode == 2) {
                    FloatButtonBackTop(FloatButtonBackTopProps{});
                }
                if (mode == 3) {
                    FloatButtonBackTop(
                        FloatButtonBackTopProps{}.scroll(FloatButtonScrollState{-1, 1, 1}).onScrollTo([](float) {}));
                }
                if (mode == 4) {
                    FloatButtonGroup(FloatButtonGroupProps{}.placement(static_cast<FloatButtonPlacement>(99)),
                                     FloatButtonGroupContent{[] {}});
                }
                if (mode == 5) {
                    ThemeConfig config;
                    config.float_button.tokens.size = dp(std::numeric_limits<float>::infinity());
                    Theme(ThemeProps{}.config(config), ThemeContent{[] { FloatButton(); }});
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.components().component_count() == 0 && f.services.interactions().size() == 0 &&
                  f.services.float_button().mounted().empty(),
              "Invalid FloatButton did not reject/roll back");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::pair<std::string_view, void (*)()> cases[] = {{"basic", basic},
                                                                 {"positioning_drag", positioning_drag},
                                                                 {"anchors", anchors},
                                                                 {"groups", groups},
                                                                 {"controlled_hover", controlled_hover},
                                                                 {"back_top", back_top},
                                                                 {"reduced_lifecycle", reduced_lifecycle},
                                                                 {"off_page", off_page},
                                                                 {"validation", validation}};
        for (const auto& [name, test] : cases) {
            if (argc < 2 || name == argv[1]) {
                test();
                std::cout << name << " passed\n";
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
