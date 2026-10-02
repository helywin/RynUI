#include "support/input_fixture.hpp"
#include "component/space_compact.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .001F;
}

float clipped_coverage(const graphics::RoundedEffectInstance& effect, runtime::Point point, float aa = 1) {
    if (effect.geometry.ancestor_clip) {
        const auto clip = effect.geometry.ancestor_clip->bounds;
        if (point.x < clip.x || point.x >= clip.x + clip.width || point.y < clip.y || point.y >= clip.y + clip.height) {
            return 0;
        }
    }
    return graphics::rounded_effect_coverage(point, effect, aa);
}

void size_corners_direction_and_lifetime() {
    Fixture fixture;
    Signal<ControlSize> size{ControlSize::Large};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    Signal<SpaceOrientation> orientation{SpaceOrientation::Horizontal};
    Signal<bool> block{false};
    int runs{};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(SpaceCompactProps{}.size(size).direction(direction).orientation(orientation).block(block),
                     SpaceCompactContent{[&] {
                         ++runs;
                         Button(ButtonProps{}, [] { Text(u8"第一"); });
                         Button(ButtonProps{}.size(ControlSize::Middle), [] { Text(u8"显式"); });
                         Button(ButtonProps{}, [] { Text(u8"最后"); });
                     }});
    }});
    fixture.synchronize();
    auto& host = fixture.services.components();
    const auto component = host.root_components().front();
    const auto group = host.state<detail::SpaceCompactState>(component)->context;
    const auto buttons = fixture.buttons.mounted_buttons();
    const auto first = buttons[0];
    const auto middle = buttons[1];
    const auto last = buttons[2];
    require(fixture.buttons.snapshot(first.component).size == ControlSize::Large &&
                fixture.buttons.snapshot(middle.component).size == ControlSize::Middle &&
                fixture.buttons.snapshot(last.component).size == ControlSize::Large,
            "Compact size inheritance or explicit Middle priority failed");
    require(fixture.buttons.snapshot(first.component).compact_corners == std::array{true, false, false, true} &&
                fixture.buttons.snapshot(middle.component).compact_corners == std::array{false, false, false, false} &&
                fixture.buttons.snapshot(last.component).compact_corners == std::array{false, true, true, false},
            "Compact horizontal exterior corner masks differ");
    const auto first_bounds = fixture.nodes.require(first.node).bounds;
    require(near(fixture.nodes.require(middle.node).bounds.x, first_bounds.x + first_bounds.width - 1),
            "Compact does not overlap one shared border");
    const auto measured = fixture.nodes.require(first.node).measure_count;
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize();
    require(fixture.nodes.require(first.node).bounds.x > fixture.nodes.require(last.node).bounds.x &&
                fixture.nodes.require(first.node).measure_count == measured && runs == 1 &&
                fixture.buttons.snapshot(first.component).compact_corners == std::array{false, true, true, false},
            "Compact RTL repeated measurement or retained wrong exterior corners");
    orientation.set(SpaceOrientation::Vertical);
    fixture.synchronize();
    require(fixture.buttons.snapshot(first.component).compact_corners == std::array{true, true, false, false} &&
                fixture.buttons.snapshot(last.component).compact_corners == std::array{false, false, true, true} &&
                near(fixture.nodes.require(middle.node).bounds.y,
                     fixture.nodes.require(first.node).bounds.y + fixture.nodes.require(first.node).bounds.height - 1),
            "Compact vertical corners or shared seam failed");
    size.set(ControlSize::Small);
    block.set(true);
    fixture.synchronize();
    require(fixture.buttons.snapshot(first.component).size == ControlSize::Small &&
                fixture.buttons.snapshot(middle.component).size == ControlSize::Middle &&
                near(fixture.nodes.require(host.root(component)).bounds.width, 320) &&
                near(fixture.nodes.require(first.node).bounds.width, 320),
            "Compact reactive size/block failed");
    require(fixture.buttons.destroy(first.component), "Compact deletion failed");
    fixture.synchronize();
    require(fixture.buttons.snapshot(middle.component).compact_corners == std::array{true, true, false, false},
            "Compact deletion did not expose new first corners");
    static_cast<void>(fixture.layout.layout(host.root(component), layout::Constraints::fixed(0, 0)));
    fixture.buttons.dispose();
    require(fixture.nodes.size() == 0 && fixture.services.surfaces().size() == 0 &&
                fixture.services.interactions().size() == 0 && fixture.buttons.rounded_effects().live_count() == 0,
            "Compact lifetime leaked surface or seam effects");
    size.set(ControlSize::Large);
    direction.set(FlexDirection::LeftToRight);
    require(group->seams().empty(), "disposed Compact retained seam commands");
}

void nested_variants_and_status_seam() {
    Fixture fixture;
    Signal<bool> disabled{false};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(SpaceCompactProps{}.size(ControlSize::Large).direction(direction), SpaceCompactContent{[&] {
                         SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {
                                          Button(ButtonProps{}.variant(ButtonVariant::Dashed),
                                                 [] { Text(u8"嵌套一"); });
                                          Button(ButtonProps{}, [] { Text(u8"嵌套二"); });
                                      }});
                         Button(ButtonProps{}.disabled(disabled), [] { Text(u8"外层末项"); });
                     }});
        for (const auto variant : {ButtonVariant::Outlined, ButtonVariant::Dashed, ButtonVariant::Solid,
                                   ButtonVariant::Filled, ButtonVariant::Text, ButtonVariant::Link}) {
            SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[variant] {
                             Button(ButtonProps{}.variant(variant), [] { Text(u8"单项"); });
                         }});
        }
        SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {}});
    }});
    fixture.synchronize(640, {0, 0, 640, 240});
    const auto buttons = fixture.buttons.mounted_buttons();
    require(
        fixture.buttons.snapshot(buttons[0].component).size == ControlSize::Large &&
            fixture.buttons.snapshot(buttons[1].component).size == ControlSize::Large &&
            fixture.buttons.snapshot(buttons[0].component).compact_corners == std::array{true, false, false, true} &&
            fixture.buttons.snapshot(buttons[1].component).compact_corners == std::array{false, false, false, false} &&
            fixture.buttons.snapshot(buttons[0].component).dashed_effects > 0,
        "nested Compact size/corner intersection or dashed border failed");
    for (std::size_t i = 3; i < buttons.size(); ++i) {
        require(fixture.buttons.snapshot(buttons[i].component).compact_corners == std::array{true, true, true, true},
                "single-item Compact removed exterior corners");
    }
    auto& host = fixture.services.components();
    const auto nested = host.children(host.root_components().front()).front();
    const auto context = host.state<detail::SpaceCompactState>(nested)->context;
    const auto outer = host.state<detail::SpaceCompactState>(host.root_components().front())->context;
    require(context->seams().size() == 4, "nested Compact seam missing");
    require(fixture.services.focus().request_focus(buttons[0].interaction, input::FocusModality::keyboard),
            "Compact focus failed");
    fixture.synchronize(640, {0, 0, 640, 240});
    require(context->seams().front().material.color ==
                fixture.buttons.snapshot(buttons[0].component).presentation_border,
            "focused seam lost to later normal control");
    const auto bounds = fixture.nodes.require(buttons[1].node).bounds;
    fixture.services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::move,
                                         input::PointerButton::none, bounds.x + bounds.width / 2,
                                         bounds.y + bounds.height / 2});
    fixture.synchronize(640, {0, 0, 640, 240});
    require(context->seams().front().material.color ==
                fixture.buttons.snapshot(buttons[1].component).presentation_border,
            "hover seam did not take priority over focus");
    require(outer->seams().size() == 4 && outer->seams().front().material.color ==
                                              fixture.buttons.snapshot(buttons[1].component).presentation_border,
            "nested hovered border lost priority at the outer group seam");
    const auto clip = context->seams().front().geometry.ancestor_clip->bounds;
    float coverage{};
    for (const auto& effect : context->seams()) {
        coverage += clipped_coverage(effect, {clip.x + clip.width / 2, clip.y + clip.height / 2}, .01F);
    }
    require(coverage > .99F, "shared seam draws outside the overlapping border");
    const auto measured = fixture.nodes.require(buttons[0].node).measure_count;
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize(640, {0, 0, 640, 240});
    require(fixture.nodes.require(buttons[0].node).measure_count == measured,
            "nested Compact RTL repeated measurement");
    disabled.set(true);
    fixture.synchronize();
}

void empty_nested_capture_loading_and_finite_motion() {
    Fixture fixture;
    fixture.buttons.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> loading{false};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[&] {
                         SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {}});
                         Button(ButtonProps{}.loading(loading), [] { Text(u8"加载"); });
                         SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {}});
                         Button(ButtonProps{}.danger(true), [] { Text(u8"删除"); });
                     }});
    }});
    fixture.synchronize();
    const auto first = fixture.buttons.mounted_buttons()[0];
    const auto last = fixture.buttons.mounted_buttons()[1];
    auto& components = fixture.services.components();
    const auto context = components.state<detail::SpaceCompactState>(components.root_components().front())->context;
    require(fixture.buttons.snapshot(first.component).compact_corners == std::array{true, false, false, true},
            "empty nested Compact consumed first exterior corners");
    require(context->seams().size() == 4, "empty nested Compact interrupted adjacent shared seam");
    const auto bounds = fixture.nodes.require(last.node).bounds;
    const auto point = runtime::Point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    fixture.services.pointer().dispatch(
        {input::PointerIdentity::mouse(), input::PointerAction::down, input::PointerButton::primary, point.x, point.y});
    require(fixture.services.pointer().state(input::PointerIdentity::mouse())->capture == last.interaction,
            "Compact control did not capture pointer");
    require(fixture.buttons.destroy(last.component), "captured Compact control deletion failed");
    fixture.synchronize();
    require(!fixture.services.pointer().state(input::PointerIdentity::mouse())->capture &&
                fixture.buttons.snapshot(first.component).compact_corners == std::array{true, true, true, true},
            "captured deletion retained capture or internal corners");
    loading.set(true);
    static_cast<void>(fixture.buttons.tick_animations(animation::AnimationTime::microseconds(250'000)));
    fixture.synchronize();
    std::size_t visible_segments{};
    for (const auto& quad : fixture.services.surfaces().instances().instances()) {
        visible_segments += quad.opacity > 0 ? 1 : 0;
    }
    require(visible_segments == 8 && fixture.buttons.snapshot(first.component).spinner_running,
            "Compact background covered or duplicated the loading spinner");
    loading.set(false);
    static_cast<void>(fixture.buttons.tick_animations(animation::AnimationTime::microseconds(1'000'000)));
    fixture.synchronize();
    const auto current = fixture.nodes.require(first.node).bounds;
    const auto center = runtime::Point{current.x + current.width / 2, current.y + current.height / 2};
    fixture.buttons.set_animation_time(animation::AnimationTime::microseconds(1'000'000));
    fixture.services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::down,
                                         input::PointerButton::primary, center.x, center.y});
    fixture.services.pointer().dispatch(
        {input::PointerIdentity::mouse(), input::PointerAction::up, input::PointerButton::primary, center.x, center.y});
    require(fixture.buttons.snapshot(first.component).wave_active, "Compact activation did not start wave");
    static_cast<void>(fixture.buttons.tick_animations(animation::AnimationTime::microseconds(3'000'000)));
    fixture.synchronize();
    require(!fixture.buttons.snapshot(first.component).wave_active && !fixture.buttons.next_deadline(),
            "Compact wave or loading retained an endless deadline");
    const auto updates = fixture.services.surfaces().effects().diagnostics().geometry_updates;
    fixture.synchronize();
    fixture.synchronize();
    require(fixture.services.surfaces().effects().diagnostics().geometry_updates == updates,
            "idle Compact repeated effect geometry updates");
}

void corner_shadow_coverage_and_retained_geometry() {
    const graphics::LogicalRoundedRect shape{{20, 20, 80, 40}, 8};
    const ShadowLayer layer{ShadowKind::outer, {2, 3}, 8, 1, Color{0, 0, 0, .3F}};
    const auto regular = graphics::make_shadow_effect(shape, layer);
    const auto rounded = graphics::make_corner_shadow_effects(shape, {true, true, true, true}, layer);
    const auto joined = graphics::make_corner_shadow_effects(shape, {true, false, false, true}, layer);
    require(joined[0].geometry.shape.radius == 8 && joined[1].geometry.shape.radius == 0 &&
                joined[2].geometry.shape.radius == 0 && joined[3].geometry.shape.radius == 8,
            "corner shadows retain interior rounding");
    for (float y = 10.25F; y < 80; y += 3.5F) {
        for (float x = 10.25F; x < 120; x += 3.5F) {
            float coverage{};
            for (const auto& effect : rounded) {
                coverage += clipped_coverage(effect, {x, y});
            }
            require(near(coverage, graphics::rounded_effect_coverage({x, y}, regular)),
                    "quadrant shadows duplicate or lose regular coverage");
        }
    }
}

void overlay_controls_do_not_join_the_trigger_group() {
    Fixture fixture;
    fixture.buttons.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(
            SpaceCompactProps{}.size(ControlSize::Large), SpaceCompactContent{[&] {
                Tooltip(TooltipProps{}.open(open),
                        TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"触发"); }); }}, TooltipTitle{[] {
                            Flex(FlexProps{}.vertical(true), FlexContent{[] {
                                     Button(ButtonProps{}.loading(true), [] { Text(u8"浮层操作"); });
                                     SpaceCompact(SpaceCompactProps{}.size(ControlSize::Small), SpaceCompactContent{[] {
                                                      Flex(FlexProps{}, FlexContent{[] {
                                                               Button(ButtonProps{}, [] { Text(u8"浮层自己的组合"); });
                                                           }});
                                                  }});
                                 }});
                        }});
                Button(ButtonProps{}, [] { Text(u8"末项"); });
            }});
    }});
    fixture.synchronize();
    const auto buttons = fixture.buttons.mounted_buttons();
    require(fixture.buttons.snapshot(buttons[0].component).size == ControlSize::Large &&
                fixture.buttons.snapshot(buttons[0].component).compact_corners ==
                    std::array{true, false, false, true} &&
                !fixture.buttons.snapshot(buttons[1].component).compact &&
                fixture.buttons.snapshot(buttons[1].component).size == ControlSize::Middle &&
                fixture.buttons.snapshot(buttons[2].component).compact &&
                fixture.buttons.snapshot(buttons[2].component).size == ControlSize::Small &&
                fixture.buttons.snapshot(buttons[3].component).compact_corners == std::array{false, true, true, false},
            "Tooltip popup controls joined the retained Compact trigger group");
    require(!fixture.buttons.snapshot(buttons[1].component).spinner_running,
            "inactive popup retained loading animation");
    open.set(true);
    fixture.synchronize();
    require(fixture.buttons.snapshot(buttons[1].component).spinner_running &&
                fixture.nodes.require(buttons[1].node).bounds.width > 0,
            "visible popup did not position its Button or resume loading");
    open.set(false);
    fixture.synchronize();
    static_cast<void>(fixture.buttons.tick_animations(animation::AnimationTime::microseconds(1'000'000)));
    fixture.synchronize();
    require(!fixture.buttons.snapshot(buttons[1].component).spinner_running && !fixture.buttons.next_deadline(),
            "closed popup retained an endless Button deadline");
}

void compact_editors_keep_sessions_and_owned_slots() {
    Fixture fixture;
    Signal<ControlSize> size{ControlSize::Large};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    int slots{};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(SpaceCompactProps{}.size(size).direction(direction).block(true), SpaceCompactContent{[&] {
                         Input(InputProps{}.defaultValue(u8"原文").layout(LayoutStyle{}.flex_grow(1).min_width(dp(0))),
                               InputPrefix{[&] {
                                   ++slots;
                                   Button(ButtonProps{}, [] { Text(u8"内部"); });
                               }});
                         Password(PasswordProps{}.defaultValue(u8"秘密"));
                         Input(InputProps{}.size(ControlSize::Middle).defaultValue(u8"显式"));
                         Button(ButtonProps{}, [] { Text(u8"末项"); });
                     }});
    }});
    fixture.synchronize(960, {0, 0, 960, 240});
    const auto first = fixture.inputs.mounted_inputs()[0];
    const auto password = fixture.inputs.mounted_inputs()[1];
    const auto explicit_middle = fixture.inputs.mounted_inputs()[2];
    require(fixture.inputs.size(first.component) == ControlSize::Large &&
                fixture.inputs.size(password.component) == ControlSize::Large &&
                fixture.inputs.size(explicit_middle.component) == ControlSize::Middle &&
                !fixture.buttons.snapshot(fixture.buttons.mounted_buttons()[0].component).compact,
            "Input/Password inheritance or owned slot isolation failed");
    require(fixture.inputs.compact_corners(first.component) == std::array{true, false, false, true} &&
                fixture.inputs.compact_corners(password.component) == std::array{false, false, false, false},
            "Compact Input retained internal rounded corners");
    require(fixture.services.focus().request_focus(first.interaction, input::FocusModality::keyboard),
            "Compact Input cannot focus");
    const auto stamp = fixture.inputs.sessions().active();
    auto& editor = fixture.inputs.editors().require(first.editor);
    require(bool(editor.move(input::TextCaretMove::end)), "Compact caret did not move");
    require(bool(fixture.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
            "Compact IME did not start");
    const auto effects = fixture.services.surfaces().effects().live_count();
    size.set(ControlSize::Small);
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize(960, {0, 0, 960, 240});
    require(fixture.inputs.sessions().active() == stamp && editor.composition().active &&
                fixture.inputs.mounted_inputs()[0].editor == first.editor && slots == 1 &&
                fixture.inputs.size(first.component) == ControlSize::Small &&
                fixture.inputs.compact_corners(first.component) == std::array{false, true, true, false} &&
                fixture.services.surfaces().effects().live_count() == effects,
            "Compact size/direction replaced editor, IME, slot or effect identity");
    require(bool(fixture.inputs.dispatch(input::TextCommitted{String{u8"你"}, stamp})) && editor.value() == "原文你",
            "Compact Unicode commit failed");
    fixture.buttons.dispose();
    require(!fixture.inputs.sessions().active().valid() && fixture.inputs.editors().size() == 0 &&
                fixture.platform.starts == fixture.platform.stops,
            "Compact editor disposal leaked its session");
}

void compact_search_flex_distribution_and_popup_input() {
    Fixture fixture;
    Signal<bool> open{false};
    fixture.buttons.mount(Content{[&] {
        SpaceCompact(SpaceCompactProps{}.size(ControlSize::Large).block(true), SpaceCompactContent{[] {
                         Search(SearchProps{}.layout(LayoutStyle{}.flex_grow(1).min_width(dp(0))));
                         Button(ButtonProps{}, [] { Text(u8"更多"); });
                     }});
        Tooltip(TooltipProps{}.open(open), TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"编辑"); }); }},
                TooltipTitle{[] { Input(InputProps{}.defaultValue(u8"浮层输入")); }});
    }});
    fixture.synchronize(500, {0, 0, 500, 240});
    const auto search = fixture.inputs.mounted_inputs()[0];
    const auto popup = fixture.inputs.mounted_inputs()[1];
    const auto action = fixture.buttons.mounted_buttons()[0];
    const auto last = fixture.buttons.mounted_buttons()[1];
    const auto input_bounds = fixture.nodes.require(search.node).bounds;
    const auto action_bounds = fixture.nodes.require(action.node).bounds;
    const auto last_bounds = fixture.nodes.require(last.node).bounds;
    require(fixture.inputs.size(search.component) == ControlSize::Large &&
                fixture.buttons.snapshot(action.component).size == ControlSize::Large &&
                near(input_bounds.x + input_bounds.width - 1, action_bounds.x) &&
                near(action_bounds.x + action_bounds.width - 1, last_bounds.x) &&
                near(last_bounds.x + last_bounds.width, 500),
            "Compact Search inherited wrong size or did not distribute its width/shared borders");
    require(!fixture.services.focus().request_focus(popup.interaction, input::FocusModality::keyboard),
            "hidden popup Input accepted focus");
    open.set(true);
    fixture.synchronize(500, {0, 0, 500, 240});
    require(fixture.services.focus().request_focus(popup.interaction, input::FocusModality::keyboard),
            "visible popup Input rejected focus");
    const auto stamp = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
            "popup composition failed");
    const auto bounds = fixture.nodes.require(popup.node).bounds;
    fixture.services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::down,
                                         input::PointerButton::primary, bounds.x + bounds.width / 2,
                                         bounds.y + bounds.height / 2});
    open.set(false);
    require(!fixture.inputs.sessions().active().valid() && !fixture.services.focus().state().focused &&
                !fixture.services.pointer().state(input::PointerIdentity::mouse())->capture &&
                !fixture.inputs.editors().require(popup.editor).composition().active,
            "closing popup retained editing session, IME, focus or capture");
    fixture.synchronize();
    require(!fixture.inputs.dispatch(input::TextCommitted{String{u8"迟到"}, stamp}),
            "closed popup accepted stale commit");
}
} // namespace

int main() {
    try {
        size_corners_direction_and_lifetime();
        nested_variants_and_status_seam();
        empty_nested_capture_loading_and_finite_motion();
        corner_shadow_coverage_and_retained_geometry();
        overlay_controls_do_not_join_the_trigger_group();
        compact_editors_keep_sessions_and_owned_slots();
        compact_search_flex_distribution_and_popup_input();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
