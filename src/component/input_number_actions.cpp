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
    input::PressableBehavior press;
    bool hovered{};
    bool enabled{};
    bool visible{};
    bool down{};
    float border_width{};
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
                                              std::function<void(InputNumberStepDirection)> activate) {
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
                const auto id = inner.mount_component<NumberActionState>();
                result.buttons[i] = id;
                auto& state = inner.state<NumberActionState>(id);
                state.node = inner.root(id);
                state.down = i == 1;
                inner.on_resource_cleanup(id, [&host, id] {
                    if (auto* s = host.components().state<NumberActionState>(id)) {
                        host.pointer().cancel_interaction(s->interaction);
                        host.focus().cancel_interaction(s->interaction);
                        host.interactions().remove(s->interaction);
                        host.surfaces().destroy(s->surface);
                        host.layout().remove_layout(s->node);
                    }
                });
                state.interaction = host.interactions().create({id, state.node, parent, false, false, {}});
                const auto fragment =
                    inner.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
                graphics::QuadInstance quad;
                quad.opacity = 0;
                const std::array initial{quad, quad, quad};
                state.surface = host.surfaces().create(id, state.node, fragment, state.interaction, initial);
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
                    if (pressed.activate) {
                        activate(direction);
                    }
                };
                host.interactions().set_handlers(state.interaction, std::move(pointer));
                layout::FlexLayout box;
                box.align = layout::FlexAlign::center;
                box.justify = layout::FlexJustify::center;
                host.layout().set_layout(state.node, box);
                inner.mount_slot_with_semantic_text_style(
                    id, Content{[i] {
                        Icon(IconProps{}.source(IconSource{i == 0 ? IconName::UpOutlined : IconName::DownOutlined}));
                    }},
                    Prop<runtime::SemanticForeground>{state.foreground},
                    Prop<runtime::SemanticTypography>{state.typography});
            }
        }});
    return result;
}

void configure_input_number_actions(WindowComponentServices& host, const InputNumberActions& actions,
                                    const InputNumberThemeToken& token, float height, float border_width, bool visible,
                                    std::array<bool, 2> enabled, InputVariant variant) {
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
        s->border_width = border_width;
        s->enabled = visible && enabled[i];
        if (!s->enabled) {
            s->hovered = false;
            static_cast<void>(s->press.reset());
            host.pointer().cancel_interaction(s->interaction);
        }
        host.interactions().set_eligible(s->interaction, s->enabled);
        auto& node = host.nodes().require(s->node);
        const auto width = visible ? token.handle_width : 0;
        const auto row_height = visible ? height * .5F : 0;
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
        if (!s) {
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
        const std::array quads{
            quad(bounds, color),
            quad({bounds.x, bounds.y, std::min(s->border_width, bounds.width), bounds.height},
                 s->token.handle_border_color),
            quad({bounds.x, bounds.y, bounds.width, s->down ? std::min(s->border_width, bounds.height) : 0.0F},
                 s->token.handle_border_color)};
        static_cast<void>(host.surfaces().update(s->surface, quads));
        component::RetainedSurfaceEffects effects;
        effects.shape = {bounds, 0};
        effects.translation = node.translation;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(host.surfaces().update_effects(s->surface, effects));
    }
}

} // namespace ryn::detail
