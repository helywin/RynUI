#include "component/tooltip_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local TooltipComponentHost* active_tooltips{};

struct TooltipState final {
    runtime::ComponentId component;
    runtime::ComponentId trigger;
    runtime::ComponentId popup;
    runtime::ComponentId arrow_component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId body;
    Signal<String> arrow_content{String{u8"\uF000"}};
    Signal<runtime::SemanticForeground> arrow_foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> arrow_typography{runtime::SemanticTypography{}};
    Signal<String> title{String{}};
    Signal<runtime::SemanticForeground> foreground{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    bool controlled{};
    bool rich_title{};
    bool available{true};
    bool open{};
    bool disabled{};
    bool hovered{};
    bool observed{};
    bool dismissed{};
    bool arrow{true};
    bool point_at_center{};
    bool adjust{true};
    bool action_open{};
    std::optional<runtime::Point> context_anchor;
    bool needs_measure{true};
    std::optional<bool> requested;
    std::optional<animation::AnimationTime> deadline;
    bool pending_open{};
    Duration enter;
    Duration leave;
    TooltipPlacement placement{TooltipPlacement::Top};
    TooltipTriggerMode mode{TooltipTriggerMode::HoverFocus};
    TooltipTriggers triggers{true, true, false, false};
    TooltipSnapshot geometry;
    runtime::Size viewport;
    runtime::Size popup_size;
    std::function<void(bool)> callback;
    theme_runtime::Subscription colors;
    theme_runtime::Subscription metrics;
    theme_runtime::Subscription fonts;
    theme_runtime::Subscription effects;
    theme_runtime::Subscription order;
};

void validate_placement(TooltipPlacement placement) {
    if (placement < TooltipPlacement::Top || placement > TooltipPlacement::RightBottom) {
        throw std::invalid_argument("Tooltip placement is invalid");
    }
}

void validate_mode(TooltipTriggerMode mode) {
    if (mode < TooltipTriggerMode::Manual || mode > TooltipTriggerMode::ContextMenu) {
        throw std::invalid_argument("Tooltip trigger mode is invalid");
    }
}

TooltipTriggers trigger_actions(TooltipTriggerMode mode) {
    validate_mode(mode);
    return {mode == TooltipTriggerMode::Hover || mode == TooltipTriggerMode::HoverFocus,
            mode == TooltipTriggerMode::Focus || mode == TooltipTriggerMode::HoverFocus,
            mode == TooltipTriggerMode::Click, mode == TooltipTriggerMode::ContextMenu};
}

bool has_title(const TooltipState& state) {
    return state.available && (state.rich_title || !state.title.get().empty());
}

void validate_delay(Duration delay) {
    if (delay.count_milliseconds() > 86400000) {
        throw std::invalid_argument("Tooltip delay cannot exceed one day");
    }
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    if (!services.components().contains(child) || !services.components().contains(ancestor)) {
        return false;
    }
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

int side(TooltipPlacement placement) {
    return static_cast<int>(placement) / 3;
}

runtime::Rect candidate(runtime::Rect anchor, runtime::Size popup, TooltipPlacement placement, float distance,
                        bool point_at_center, float arrow_inset) {
    const int axis = side(placement);
    const int alignment = static_cast<int>(placement) % 3;
    float x = anchor.x + (anchor.width - popup.width) / 2;
    float y = anchor.y + (anchor.height - popup.height) / 2;
    if (axis < 2) {
        if (alignment) {
            const float inset = std::min(arrow_inset, popup.width / 2);
            x = point_at_center  ? anchor.x + anchor.width / 2 - (alignment == 1 ? inset : popup.width - inset)
                : alignment == 1 ? anchor.x
                                 : anchor.x + anchor.width - popup.width;
        }
        y = axis == 0 ? anchor.y - popup.height - distance : anchor.y + anchor.height + distance;
    } else {
        if (alignment) {
            const float inset = std::min(arrow_inset, popup.height / 2);
            y = point_at_center  ? anchor.y + anchor.height / 2 - (alignment == 1 ? inset : popup.height - inset)
                : alignment == 1 ? anchor.y
                                 : anchor.y + anchor.height - popup.height;
        }
        x = axis == 2 ? anchor.x - popup.width - distance : anchor.x + anchor.width + distance;
    }
    return {x, y, popup.width, popup.height};
}

runtime::Rect intersect(runtime::Rect value, runtime::Rect clip) {
    const float right = std::min(value.x + value.width, clip.x + clip.width);
    const float bottom = std::min(value.y + value.height, clip.y + clip.height);
    value.x = std::max(value.x, clip.x);
    value.y = std::max(value.y, clip.y);
    value.width = std::max(0.0F, right - value.x);
    value.height = std::max(0.0F, bottom - value.y);
    return value;
}
} // namespace

TooltipSnapshot position_tooltip(runtime::Rect anchor, runtime::Size popup, runtime::Rect viewport,
                                 TooltipPlacement placement, float distance, bool adjust, bool point_at_center,
                                 float arrow_inset) {
    validate_placement(placement);
    for (float value : {anchor.x, anchor.y, anchor.width, anchor.height, popup.width, popup.height, viewport.x,
                        viewport.y, viewport.width, viewport.height, distance, arrow_inset, anchor.x + anchor.width,
                        anchor.y + anchor.height, viewport.x + viewport.width, viewport.y + viewport.height}) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Tooltip geometry must be finite");
        }
    }
    if (anchor.width < 0 || anchor.height < 0 || popup.width < 0 || popup.height < 0 || viewport.width < 0 ||
        viewport.height < 0 || distance < 0 || arrow_inset < 0) {
        throw std::invalid_argument("Tooltip extents must be non-negative");
    }
    popup.width = std::min(popup.width, viewport.width);
    popup.height = std::min(popup.height, viewport.height);
    auto bounds = candidate(anchor, popup, placement, distance, point_at_center, arrow_inset);
    if (adjust) {
        const int axis = side(placement);
        const auto opposite = static_cast<TooltipPlacement>((axis ^ 1) * 3 + static_cast<int>(placement) % 3);
        const auto alternate = candidate(anchor, popup, opposite, distance, point_at_center, arrow_inset);
        const auto overflow = [viewport, axis](runtime::Rect value) {
            return axis < 2 ? std::max(viewport.y - value.y, 0.0F) +
                                  std::max(value.y + value.height - viewport.y - viewport.height, 0.0F)
                            : std::max(viewport.x - value.x, 0.0F) +
                                  std::max(value.x + value.width - viewport.x - viewport.width, 0.0F);
        };
        if (overflow(alternate) < overflow(bounds)) {
            bounds = alternate;
            placement = opposite;
        }
        bounds.x = std::clamp(bounds.x, viewport.x, viewport.x + viewport.width - bounds.width);
        bounds.y = std::clamp(bounds.y, viewport.y, viewport.y + viewport.height - bounds.height);
    }
    const bool horizontal = side(placement) < 2;
    const float extent = horizontal ? bounds.width : bounds.height;
    const float inset = std::min(arrow_inset, extent / 2);
    const int alignment = static_cast<int>(placement) % 3;
    const float target = alignment    ? alignment == 1 ? inset : extent - inset
                         : horizontal ? anchor.x + anchor.width / 2 - bounds.x
                                      : anchor.y + anchor.height / 2 - bounds.y;
    return {anchor, bounds, placement, true, std::clamp(target, inset, extent - inset)};
}

TooltipComponentHost::TooltipComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TooltipComponentHost::~TooltipComponentHost() {
    services_->detach(*this);
}

void* TooltipComponentHost::begin_mount() noexcept {
    auto* previous = active_tooltips;
    active_tooltips = this;
    return previous;
}

void TooltipComponentHost::end_mount(void* previous) noexcept {
    active_tooltips = static_cast<TooltipComponentHost*>(previous);
}

void TooltipComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TooltipSnapshot TooltipComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        throw std::out_of_range("Tooltip component is stale");
    }
    return state->geometry;
}

void TooltipComponentHost::synchronize_visibility(runtime::ComponentId id) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    const bool visible = state->open && !state->disabled && has_title(*state) && window_active_;
    if (state->geometry.visible != visible) {
        state->geometry.visible = visible;
        state->needs_measure = visible;
        services_->components().set_branch_active(state->popup, visible);
        services_->components().set_branch_active(state->arrow_component, visible && state->arrow);
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
    }
    if (!window_active_ || state->disabled || !has_title(*state)) {
        state->deadline.reset();
        state->action_open = false;
        state->context_anchor.reset();
    }
}

void TooltipComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    state->deadline.reset();
    if (state->requested == open) {
        return;
    }
    const auto previous_request = state->requested;
    state->requested = open;
    const bool changed = state->open != open;
    const auto callback = state->callback;
    if (!state->controlled) {
        state->open = open;
    }
    synchronize_visibility(id);
    if ((changed || previous_request.has_value()) && callback) {
        callback(open);
    }
}

void TooltipComponentHost::observe(runtime::ComponentId id, animation::AnimationTime time) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    const auto focus = services_->focus().state();
    const auto* focused = focus.focused ? services_->interactions().find(*focus.focused) : nullptr;
    const bool keyboard_focus = focused && focus.modality == input::FocusModality::keyboard &&
                                descendant(*services_, focused->component, state->component);
    const bool desire =
        state->action_open || (state->triggers.hover && state->hovered) || (state->triggers.focus && keyboard_focus);
    if (!desire) {
        state->dismissed = false;
    }
    if (desire != state->observed) {
        state->observed = desire;
        state->deadline.reset();
        if (!state->disabled && has_title(*state) && window_active_ && !state->dismissed) {
            const auto delay = desire && keyboard_focus ? Duration{} : desire ? state->enter : state->leave;
            const auto micros = static_cast<animation::AnimationTime::rep>(delay.count_milliseconds() * 1000.0);
            const auto now = time.count_microseconds();
            if (micros > std::numeric_limits<animation::AnimationTime::rep>::max() - now) {
                throw std::overflow_error("Tooltip deadline overflow");
            }
            state->pending_open = desire;
            state->deadline = animation::AnimationTime::microseconds(now + micros);
        }
    }
    if (state->deadline && time >= *state->deadline) {
        request_open(id, state->pending_open);
    }
    synchronize_visibility(id);
}

std::size_t TooltipComponentHost::tick_auxiliary(animation::AnimationTime time) {
    if (mounted_.empty()) {
        return 0;
    }
    const auto ids = mounted_;
    std::size_t changed{};
    for (auto id : ids) {
        const auto* state = services_->components().state<TooltipState>(id);
        if (!state) {
            continue;
        }
        const bool before = state->geometry.visible;
        observe(id, time);
        state = services_->components().state<TooltipState>(id);
        if (state && state->geometry.visible != before) {
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> TooltipComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* state = services_->components().state<TooltipState>(id);
        if (state && state->deadline && (!result || *state->deadline < *result)) {
            result = state->deadline;
        }
    }
    return result;
}

void TooltipComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            if (auto* state = services_->components().state<TooltipState>(id)) {
                state->hovered = false;
                state->dismissed = true;
                state->observed = false;
                state->action_open = false;
                state->context_anchor.reset();
            }
            request_open(id, false);
        }
        synchronize_visibility(id);
    }
}

bool TooltipComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.key != input::Key::escape || event.action != input::KeyAction::down) {
        return false;
    }
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        auto* state = services_->components().state<TooltipState>(*it);
        if (state && (state->geometry.visible || (state->deadline && state->pending_open))) {
            const auto id = *it;
            state->dismissed = true;
            state->action_open = false;
            state->context_anchor.reset();
            request_open(id, false);
            return true;
        }
    }
    return false;
}

void TooltipComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                            std::optional<input::InteractionId> hit,
                                            std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* state = services_->components().state<TooltipState>(id);
        if (!state || state->disabled || !has_title(*state) || !window_active_ ||
            (!state->triggers.click && !state->triggers.context_menu)) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_trigger = record && descendant(*services_, record->component, state->trigger);
        const bool on_wrapper = hit == state->interaction;
        const auto bounds = state->geometry.bounds;
        const bool in_popup = state->geometry.visible && event.x >= bounds.x && event.y >= bounds.y &&
                              event.x < bounds.x + bounds.width && event.y < bounds.y + bounds.height;
        if (event.action == input::PointerAction::down &&
            (event.button == input::PointerButton::primary || event.button == input::PointerButton::secondary) &&
            !in_trigger && !on_wrapper && !in_popup) {
            if (state->open || state->action_open || state->requested == true) {
                state->action_open = false;
                state->context_anchor.reset();
                state->dismissed = true;
                request_open(id, false);
            }
            continue;
        }
        if (event.action != input::PointerAction::up || (!in_trigger && !on_wrapper)) {
            continue;
        }
        if (event.button == input::PointerButton::primary && state->triggers.click && hit && origin == hit) {
            const bool next = !state->requested.value_or(state->open);
            state->action_open = next;
            state->observed = next;
            state->dismissed = !next;
            state->context_anchor.reset();
            request_open(id, next);
        } else if (event.button == input::PointerButton::secondary && state->triggers.context_menu) {
            state->context_anchor = runtime::Point{event.x, event.y};
            state->action_open = true;
            state->observed = true;
            state->dismissed = false;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
            request_open(id, true);
        }
    }
}

void TooltipComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.tooltip();
    state->foreground.set({token.text.red(), token.text.green(), token.text.blue(), token.text.alpha()});
    state->typography.set(
        {theme.typography().font_family, theme.typography().font_weight, false, token.font_size, token.line_height});
    state->arrow_foreground.set(
        {token.background.red(), token.background.green(), token.background.blue(), token.background.alpha()});
    if (geometry) {
        state->needs_measure = true;
    }
    if (services_->components().set_window_layer(state->popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    if (services_->components().set_window_layer(state->arrow_component, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(state->node, runtime::DirtyFlags::Material);
}

void TooltipComponentHost::mount(const TooltipProps& props, const TooltipTrigger& trigger, const TooltipTitle* title) {
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("Tooltip cannot combine open and defaultOpen");
    }
    if ((title && props.explicit_title_) || (props.triggers_ && props.explicit_trigger_)) {
        throw std::invalid_argument("Tooltip title or trigger declarations conflict");
    }
    validate_placement(read_prop(props.placement_));
    validate_mode(read_prop(props.trigger_));
    validate_delay(read_prop(props.enter_));
    validate_delay(read_prop(props.leave_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TooltipState>();
    auto& state = build.state<TooltipState>(id);
    state.component = id;
    state.node = build.root(id);
    state.controlled = props.open_.has_value();
    state.rich_title = title != nullptr;
    state.available = read_prop(props.available_);
    state.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    state.disabled = read_prop(props.disabled_);
    state.title.set(read_prop(props.title_));
    state.placement = read_prop(props.placement_);
    state.mode = read_prop(props.trigger_);
    state.triggers = props.triggers_ ? read_prop(*props.triggers_) : trigger_actions(state.mode);
    state.arrow = read_prop(props.arrow_);
    state.point_at_center = read_prop(props.point_at_center_);
    state.adjust = read_prop(props.adjust_);
    state.enter = read_prop(props.enter_);
    state.leave = read_prop(props.leave_);
    state.callback = props.on_open_;
    runtime::connect_layout_style(build.scope(id), props.layout_, state.node, services.nodes(), services.dirty());
    state.interaction = services.interactions().create({id, state.node, {}, true, false, {}, false});
    input::InteractionHandlers handlers;
    handlers.target = [this, id](input::PointerDispatchContext& event) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            if (event.kind() == input::PointerEventKind::enter) {
                state->hovered = true;
            }
            if (event.kind() == input::PointerEventKind::leave || event.kind() == input::PointerEventKind::cancel) {
                state->hovered = false;
            }
        }
    };
    services.interactions().set_handlers(state.interaction, std::move(handlers));
    const auto hit = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit, {}, state.interaction);
    build.on_resource_cleanup(id, [&services, node = state.node, interaction = state.interaction, hit] {
        services.pointer().cancel_interaction(interaction);
        services.interactions().remove(interaction);
        services.scene_composer().remove_fragment(hit);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            state.trigger = children.mount_component<int>(0);
            const auto trigger_node = children.root(state.trigger);
            services.layout().set_layout(trigger_node, layout::BoxLayout{});
            children.on_resource_cleanup(state.trigger,
                                         [&services, trigger_node] { services.layout().remove_layout(trigger_node); });
            children.mount_slot(state.trigger, trigger);
            state.popup = children.mount_component<int>(0);
            const auto popup_node = children.root(state.popup);
            services.layout().set_layout(popup_node, layout::BoxLayout{});
            services.components().set_window_layer(state.popup,
                                                   build.theme_scope()->snapshot().tooltip().z_index_popup);
            const auto popup_interaction =
                services.interactions().create({state.popup, popup_node, {}, true, false, {}, false});
            const auto popup_hit =
                children.register_scene_fragment(state.popup, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(popup_hit, {}, popup_interaction);
            children.on_resource_cleanup(state.popup, [&services, popup_hit, popup_interaction] {
                services.pointer().cancel_interaction(popup_interaction);
                services.interactions().remove(popup_interaction);
                services.scene_composer().remove_fragment(popup_hit);
            });
            const auto body =
                children.register_scene_fragment(state.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            state.body =
                services.surfaces().create_surface(state.popup, popup_node, body, std::span{&initial, 1}, effects);
            children.on_resource_cleanup(state.popup, [&services, popup_node, body_id = state.body] {
                services.surfaces().destroy(body_id);
                services.layout().remove_layout(popup_node);
            });
            const Content title_content = title ? Content{SlotContentAccess::function(*title)}
                                                : Content{[&state] { ryn::Text(TextProps{}.content(state.title)); }};
            children.mount_slot_with_semantic_text_style(state.popup, title_content,
                                                         Prop<runtime::SemanticForeground>{state.foreground},
                                                         Prop<runtime::SemanticTypography>{state.typography});
            state.arrow_component = children.mount_component<int>(0);
            const auto arrow_node = children.root(state.arrow_component);
            services.layout().set_layout(arrow_node, layout::BoxLayout{});
            services.components().set_window_layer(state.arrow_component,
                                                   build.theme_scope()->snapshot().tooltip().z_index_popup);
            children.on_resource_cleanup(state.arrow_component,
                                         [&services, arrow_node] { services.layout().remove_layout(arrow_node); });
            children.mount_slot_with_semantic_text_style(
                state.arrow_component,
                Content{[&state] { mount_text_component(TextProps{}.content(state.arrow_content), true); }},
                Prop<runtime::SemanticForeground>{state.arrow_foreground},
                Prop<runtime::SemanticTypography>{state.arrow_typography});
        }});
    services.components().set_branch_active(state.popup, false);
    services.components().set_branch_active(state.arrow_component, false);
    services.layout().set_layout(
        state.node, layout::ComponentLayout{
                        [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                            const auto* state = services_->components().state<TooltipState>(id);
                            return engine.measure_child(services_->components().root(state->trigger), constraints);
                        },
                        [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                            const auto* state = services_->components().state<TooltipState>(id);
                            engine.place_child(services_->components().root(state->trigger), bounds);
                        }});
    mounted_.push_back(id);
    update_theme(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.title_, [this, id](String value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->title.set(std::move(value));
            state->needs_measure = true;
            synchronize_visibility(id);
        }
    });
    connect_prop(scope, props.available_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->available = value;
            state->observed = false;
            synchronize_visibility(id);
        }
    });
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* state = services_->components().state<TooltipState>(id)) {
                state->open = value;
                state->requested = value;
                synchronize_visibility(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->disabled = value;
            state->observed = false;
            synchronize_visibility(id);
        }
    });
    connect_prop(scope, props.placement_, [this, id](TooltipPlacement value) {
        validate_placement(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->placement = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    if (!props.triggers_) {
        connect_prop(scope, props.trigger_, [this, id](TooltipTriggerMode value) {
            validate_mode(value);
            if (auto* state = services_->components().state<TooltipState>(id)) {
                if (state->triggers == trigger_actions(value)) {
                    return;
                }
                state->mode = value;
                state->triggers = trigger_actions(value);
                state->action_open = false;
                state->context_anchor.reset();
                state->observed = false;
                state->deadline.reset();
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                request_open(id, false);
            }
        });
    }
    if (props.triggers_) {
        connect_prop(scope, *props.triggers_, [this, id](TooltipTriggers value) {
            if (auto* state = services_->components().state<TooltipState>(id)) {
                if (state->triggers == value) {
                    return;
                }
                state->triggers = value;
                state->action_open = false;
                state->context_anchor.reset();
                state->observed = false;
                state->deadline.reset();
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                request_open(id, false);
            }
        });
    }
    connect_prop(scope, props.arrow_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->arrow = value;
            services_->components().set_branch_active(state->arrow_component, state->geometry.visible && value);
            services_->mark_scene_structure_dirty();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.point_at_center_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->point_at_center = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.adjust_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->adjust = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.enter_, [this, id](Duration value) {
        validate_delay(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->enter = value;
            state->observed = false;
            state->deadline.reset();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.leave_, [this, id](Duration value) {
        validate_delay(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->leave = value;
            state->observed = false;
            state->deadline.reset();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    const auto theme = build.theme_scope();
    state.colors = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, false); },
                                  [theme] { (void)theme->tooltip_colors(); });
    state.metrics = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, true); },
                                   [theme] { (void)theme->tooltip_metrics(); });
    state.fonts = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, true); },
                                 [theme] {
                                     (void)theme->tooltip_typography();
                                     (void)theme->typography_fonts();
                                 });
    state.effects = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, false); },
                                   [theme] { (void)theme->tooltip_shadow(); });
    state.order = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, false); },
                                 [theme] { (void)theme->tooltip_order(); });
    synchronize_visibility(id);
}

void TooltipComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    if (mounted_.empty()) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        observe(id, services_->animation_time());
        auto* state = services_->components().state<TooltipState>(id);
        if (!state || !state->geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().tooltip();
        const auto popup = services_->components().root(state->popup);
        auto& engine = services_->layout();
        const bool measure = state->needs_measure || state->viewport != viewport ||
                             services_->nodes().require(popup).measure_generation != engine.generation();
        if (measure) {
            const float width = std::min(token.max_width, viewport.width);
            const float horizontal = std::min(token.padding_inline, width / 2);
            const float vertical = std::min(token.padding_block, viewport.height / 2);
            engine.set_layout(popup, layout::BoxLayout{{horizontal, vertical, horizontal, vertical}});
            state->popup_size =
                engine.measure_child(popup, {0, width, std::min(token.min_height, viewport.height), viewport.height});
            state->needs_measure = false;
            state->viewport = viewport;
        }
        const auto& node = services_->nodes().require(state->node);
        auto anchor = node.bounds;
        anchor.x += node.translation.x;
        anchor.y += node.translation.y;
        if (state->context_anchor) {
            anchor = {state->context_anchor->x, state->context_anchor->y, 0, 0};
        }
        state->geometry = position_tooltip(
            anchor, state->popup_size, clip, state->placement, token.gap + (state->arrow ? token.arrow_size : 0),
            state->adjust, state->arrow && state->point_at_center, std::max(12.0F, token.border_radius + 2.0F));
        const auto& current = services_->nodes().require(popup);
        if (measure || current.bounds != state->geometry.bounds) {
            engine.place_child(popup, state->geometry.bounds);
        }
        const int axis = side(state->geometry.placement);
        const bool horizontal = axis < 2;
        const auto bounds = state->geometry.bounds;
        const float size = std::min(token.arrow_size, (horizontal ? bounds.width : bounds.height) / 2);
        const bool arrow_visible = state->arrow && size > 0;
        if (services_->components().set_branch_active(state->arrow_component, arrow_visible)) {
            services_->mark_scene_structure_dirty();
        }
        if (arrow_visible) {
            static const String glyphs[]{String{u8"\uF000"}, String{u8"\uF001"}, String{u8"\uF002"},
                                         String{u8"\uF003"}};
            state->arrow_content.set(glyphs[axis]);
            const float font_size = std::max(1.0F, std::round(size * 2));
            state->arrow_typography.set({SystemFontFamily::ui_sans, 400, false, font_size, font_size});
            const auto arrow_node = services_->components().root(state->arrow_component);
            (void)engine.measure_child(arrow_node, {0, font_size, 0, font_size});
            runtime::Rect arrow_bounds{0, 0, font_size, font_size};
            const float half = font_size / 2;
            const float center = state->geometry.arrow_center;
            if (axis == 0) {
                arrow_bounds.x = bounds.x + center - half;
                arrow_bounds.y = bounds.y + bounds.height;
            } else if (axis == 1) {
                arrow_bounds.x = bounds.x + center - half;
                arrow_bounds.y = bounds.y - half;
            } else if (axis == 2) {
                arrow_bounds.x = bounds.x + bounds.width;
                arrow_bounds.y = bounds.y + center - half;
            } else {
                arrow_bounds.x = bounds.x - half;
                arrow_bounds.y = bounds.y + center - half;
            }
            if (services_->nodes().require(arrow_node).bounds != arrow_bounds || measure) {
                engine.place_child(arrow_node, arrow_bounds);
            }
        }
    }
}

void TooltipComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect clip{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* state = services_->components().state<TooltipState>(id);
        if (!state || !state->geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().tooltip();
        const auto bounds = state->geometry.bounds;
        const auto visible = intersect(bounds, clip);
        graphics::QuadInstance body;
        body.bounds = {visible.x, visible.y, visible.width, visible.height};
        body.color = {token.background.red(), token.background.green(), token.background.blue(),
                      token.background.alpha()};
        body.corner_radius = std::min(token.border_radius, std::min(visible.width, visible.height) / 2);
        (void)services_->surfaces().update_surface(state->body, std::span{&body, 1});
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height},
                         std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2)};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        (void)services_->surfaces().update_effects(state->body, effects);
    }
}
} // namespace ryn::detail

namespace ryn {
void Tooltip(TooltipProps props, TooltipTrigger trigger) {
    if (!detail::active_tooltips) {
        throw std::logic_error("Tooltip requires window component services");
    }
    detail::active_tooltips->mount(props, trigger);
}

void Tooltip(TooltipProps props, TooltipTrigger trigger, TooltipTitle title) {
    if (!detail::active_tooltips) {
        throw std::logic_error("Tooltip requires window component services");
    }
    detail::active_tooltips->mount(props, trigger, &title);
}
} // namespace ryn
