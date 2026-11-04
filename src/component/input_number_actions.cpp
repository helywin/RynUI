#include "component/input_number_actions.hpp"
#include "input/pressable_behavior.hpp"

#include <ryn/icon.hpp>

#include <algorithm>

namespace ryn::detail {
namespace {
struct NumberActionState final {
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId background;
    input::PressableBehavior press;
    bool hovered{};
    bool enabled{};
    bool visible{};
    bool down{};
    bool single{};
    bool placeholder{};
    std::optional<animation::AnimationTime> repeat;
    std::function<void()> activate;
    float border_width{};
    float border_radius{};
    InputVariant variant{InputVariant::Outlined};
    InputNumberThemeToken token;
    Signal<runtime::SemanticForeground> foreground{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
};

void foreground(NumberActionState& state) {
    const auto color = !state.enabled                           ? state.token.disabled_color
                       : state.hovered || state.press.pressed() ? state.token.handle_hover_color
                                                                : state.token.handle_color;
    state.foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
}
} // namespace

InputNumberActions mount_input_number_actions(WindowComponentServices& host, input::InteractionId parent,
                                              std::function<void(InputNumberStepDirection)> activate,
                                              const InputNumberControls& icons, int single) {
    auto& build = runtime::require_component_build_context();
    InputNumberActions result;
    result.group = build.mount_component<int>(0);
    const auto group_node = build.root(result.group);
    layout::FlexLayout column;
    column.direction = layout::FlexDirection::vertical;
    column.item_policy = layout::FlexItemPolicy::sequential;
    host.layout().set_layout(group_node, column);
    build.on_resource_cleanup(result.group, [&host, group_node] { host.layout().remove_layout(group_node); });
    build.mount_slot(
        result.group, Content{[&] {
            auto& inner = runtime::require_component_build_context();
            for (std::size_t i = 0; i < 2; ++i) {
                if (single >= 0 && static_cast<int>(i) != single) {
                    continue;
                }
                const auto id = inner.mount_component<NumberActionState>();
                result.buttons[i] = id;
                auto& state = inner.state<NumberActionState>(id);
                state.node = inner.root(id);
                state.down = i == 1;
                state.single = single >= 0;
                state.placeholder = !icons.enabled;
                inner.on_resource_cleanup(id, [&host, id] {
                    if (auto* s = host.components().state<NumberActionState>(id)) {
                        host.pointer().cancel_interaction(s->interaction);
                        host.focus().cancel_interaction(s->interaction);
                        host.interactions().remove(s->interaction);
                        host.surfaces().destroy(s->surface);
                        host.surfaces().destroy_content_range(s->background);
                        host.layout().remove_layout(s->node);
                    }
                });
                state.interaction = host.interactions().create({id, state.node, parent, false, false, {}});
                const auto background =
                    inner.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
                state.background = host.surfaces().create_content_range(background, {});
                const auto fragment =
                    inner.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
                graphics::QuadInstance quad;
                quad.opacity = 0;
                const std::array initial{quad, quad, quad};
                state.surface = host.surfaces().create(id, state.node, fragment, state.interaction, initial);
                state.activate = [activate,
                                  direction = i == 0 ? InputNumberStepDirection::Up : InputNumberStepDirection::Down] {
                    activate(direction);
                };
                input::InteractionHandlers pointer;
                pointer.target = [&host, id, activate,
                                  direction = i == 0 ? InputNumberStepDirection::Up : InputNumberStepDirection::Down](
                                     input::PointerDispatchContext& context) {
                    auto* s = host.components().state<NumberActionState>(id);
                    if (!s) {
                        return;
                    }
                    if (context.kind() == input::PointerEventKind::enter) {
                        s->hovered = true;
                    } else if (context.kind() == input::PointerEventKind::leave ||
                               context.kind() == input::PointerEventKind::cancel) {
                        s->hovered = false;
                    }
                    const auto pressed = s->press.dispatch(context, s->interaction, s->enabled && s->visible);
                    foreground(*s);
                    host.dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                    if (context.kind() == input::PointerEventKind::up ||
                        context.kind() == input::PointerEventKind::cancel ||
                        context.kind() == input::PointerEventKind::leave) {
                        s->repeat.reset();
                    }
                    if (context.kind() == input::PointerEventKind::down && pressed.pressed_changed &&
                        s->press.pressed()) {
                        s->repeat = host.animation_time() + animation::AnimationDuration::milliseconds(600);
                        auto callback = s->activate;
                        host.request_frame();
                        try {
                            callback();
                        } catch (...) {
                            if (auto* current = host.components().state<NumberActionState>(id)) {
                                current->repeat.reset();
                                host.pointer().cancel_interaction(current->interaction);
                            }
                            throw;
                        }
                    }
                };
                host.interactions().set_handlers(state.interaction, std::move(pointer));
                layout::FlexLayout box;
                box.align = layout::FlexAlign::center;
                box.justify = layout::FlexJustify::center;
                host.layout().set_layout(state.node, box);
                inner.mount_slot_with_semantic_text_style(
                    id, Content{[i, icons, single] {
                        if (!icons.enabled) {
                            return;
                        }
                        if (i == 0 && icons.up_icon) {
                            SlotContentAccess::function (*icons.up_icon)();
                            return;
                        }
                        if (i == 1 && icons.down_icon) {
                            SlotContentAccess::function (*icons.down_icon)();
                            return;
                        }
                        if (single >= 0) {
                            Icon(IconProps{}.source(
                                IconSource{i == 0 ? IconName::PlusOutlined : IconName::MinusOutlined}));
                            return;
                        }
                        Icon(IconProps{}.source(IconSource{i == 0 ? IconName::UpOutlined : IconName::DownOutlined}));
                    }},
                    Prop<runtime::SemanticForeground>{state.foreground},
                    Prop<runtime::SemanticTypography>{state.typography});
            }
        }});
    return result;
}

void cancel_input_number_actions(WindowComponentServices& host, const InputNumberActions& actions) {
    for (auto id : actions.buttons) {
        if (auto* s = host.components().state<NumberActionState>(id)) {
            s->repeat.reset();
            static_cast<void>(s->press.reset());
            host.pointer().cancel_interaction(s->interaction);
        }
    }
}

std::optional<animation::AnimationTime> input_number_actions_deadline(WindowComponentServices& host,
                                                                      const InputNumberActions& actions) {
    std::optional<animation::AnimationTime> deadline;
    for (auto id : actions.buttons) {
        if (auto* s = host.components().state<NumberActionState>(id);
            s && s->repeat && s->enabled && s->visible && host.interactions().component_active(id) &&
            host.focus().state().window_active && s->press.pressed()) {
            if (!deadline || *s->repeat < *deadline) {
                deadline = s->repeat;
            }
        }
    }
    return deadline;
}

std::size_t tick_input_number_actions(WindowComponentServices& host, const InputNumberActions& actions,
                                      animation::AnimationTime now) {
    std::size_t changed{};
    for (auto id : actions.buttons) {
        auto* s = host.components().state<NumberActionState>(id);
        if (!s || !s->repeat) {
            continue;
        }
        if (!s->enabled || !s->visible || !s->press.pressed() || !host.interactions().component_active(id) ||
            !host.focus().state().window_active) {
            cancel_input_number_actions(host, actions);
            continue;
        }
        if (now < *s->repeat) {
            continue;
        }
        s->repeat = now + animation::AnimationDuration::milliseconds(100);
        auto callback = s->activate;
        try {
            callback();
        } catch (...) {
            cancel_input_number_actions(host, actions);
            throw;
        }
        ++changed;
    }
    return changed;
}

void configure_input_number_actions(WindowComponentServices& host, const InputNumberActions& actions,
                                    const InputNumberThemeToken& token, float height, float border_width,
                                    float border_radius, bool visible, std::array<bool, 2> enabled,
                                    InputVariant variant, bool spinner) {
    if (!host.components().contains(actions.group)) {
        return;
    }
    if (host.components().set_branch_active(actions.group, visible)) {
        host.mark_scene_structure_dirty();
    }
    for (std::size_t i = 0; i < 2; ++i) {
        auto* s = host.components().state<NumberActionState>(actions.buttons[i]);
        if (!s) {
            continue;
        }
        const auto old = s->token;
        const bool visibility_changed = s->visible != visible;
        s->visible = visible;
        s->token = token;
        s->variant = variant;
        (void)spinner;
        s->border_width = border_width;
        s->border_radius = border_radius;
        s->enabled = visible && enabled[i] && !s->placeholder;
        if (!s->enabled) {
            s->hovered = false;
            s->repeat.reset();
            static_cast<void>(s->press.reset());
            host.pointer().cancel_interaction(s->interaction);
        }
        host.interactions().set_eligible(s->interaction, s->enabled);
        auto& node = host.nodes().require(s->node);
        const auto width = visible ? token.handle_width : 0;
        const auto row_height = visible ? height * (s->single || spinner ? 1.0F : .5F) : 0;
        if (visibility_changed || old.metrics() != token.metrics() || node.external_layout.width != width ||
            node.external_layout.height != row_height) {
            node.external_layout.width = width;
            node.external_layout.height = row_height;
            host.dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                 runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        }
        s->typography.set({SystemFontFamily::ui_sans, 400, false, token.handle_font_size, token.handle_font_size});
        foreground(*s);
        host.dirty().invalidate(s->node, runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
    }
}

void paint_input_number_actions(WindowComponentServices& host, const InputNumberActions& actions, runtime::Rect clip) {
    for (const auto id : actions.buttons) {
        const auto* s = host.components().state<NumberActionState>(id);
        if (!s || s->placeholder) {
            continue;
        }
        const auto& node = host.nodes().require(s->node);
        auto color = s->press.pressed()                   ? s->token.handle_active_background
                     : s->variant == InputVariant::Filled ? s->token.filled_handle_background
                                                          : s->token.handle_background;
        const auto quad = [&](runtime::Rect bounds, Color fill) {
            graphics::QuadInstance q;
            q.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
            q.color = {fill.red(), fill.green(), fill.blue(), fill.alpha()};
            q.opacity = s->visible ? node.opacity : 0;
            q.translation = {node.translation.x, node.translation.y};
            return q;
        };
        const auto bounds = node.bounds;
        std::array quads{
            quad(bounds, color),
            quad({bounds.x, bounds.y, std::min(s->border_width, bounds.width), bounds.height},
                 s->token.handle_border_color),
            quad({bounds.x, bounds.y, bounds.width, s->down ? std::min(s->border_width, bounds.height) : 0.0F},
                 s->token.handle_border_color)};
        quads[0].opacity = 0;
        static_cast<void>(host.surfaces().update(s->surface, quads));
        const float radius =
            std::min(std::max(0.0F, s->border_radius - s->border_width), std::min(bounds.width, bounds.height) / 2);
        const auto background = graphics::make_corner_fill_effects({bounds, radius}, {false, !s->down, s->down, false},
                                                                   color, s->visible ? node.opacity : 0,
                                                                   node.translation, graphics::EffectClip{1, clip});
        host.surfaces().update_content_effects(s->background, background);
        component::RetainedSurfaceEffects effects;
        effects.shape = {bounds, 0};
        effects.translation = node.translation;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(host.surfaces().update_effects(s->surface, effects));
    }
}

} // namespace ryn::detail
