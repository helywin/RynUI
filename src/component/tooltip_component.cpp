#include "component/tooltip_component.hpp"
#include "component/popover_component.hpp"
#include "component/popup_motion.hpp"
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
    component::RetainedSurfaceId fill;
    std::optional<graphics::TooltipArrowPath> arrow_path;
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
    bool boundary_suppressed{};
    bool arrow{true};
    bool point_at_center{};
    bool adjust{true};
    bool action_open{};
    bool animated{};
    PopupMotion motion;
    TextDirection direction{TextDirection::LeftToRight};
    Signal<TextDirection> text_direction{TextDirection::LeftToRight};
    std::optional<runtime::Point> context_anchor;
    std::optional<std::pair<input::PointerIdentity, input::InteractionId>> context_press;
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
    runtime::Rect clip;
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

TooltipPlacement directional_popup_placement(TooltipPlacement placement, TextDirection direction) {
    validate_placement(placement);
    if (direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
        direction != TextDirection::RightToLeft) {
        throw std::invalid_argument("Popup direction is invalid");
    }
    if (direction != TextDirection::RightToLeft) {
        return placement;
    }
    const int axis = side(placement);
    const int alignment = static_cast<int>(placement) % 3;
    return static_cast<TooltipPlacement>(axis < 2 ? axis * 3 + (alignment ? 3 - alignment : 0)
                                                  : (axis ^ 1) * 3 + alignment);
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

bool TooltipComponentHost::maintenance_allowed(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TooltipState>(id);
    if (!s || !s->open || s->boundary_suppressed || s->disabled || !has_title(*s) || !window_active_ ||
        !services_->interactions().component_lifetime_active(id) || !services_->components().branch_active(id)) {
        return false;
    }
    if (services_->interactions().component_active(id)) {
        return true;
    }
    if (!s->geometry.visible || !services_->components().branch_active(s->popup)) {
        return false;
    }
    const auto clip = services_->component_clip(s->popup, s->viewport, {0, 0, s->viewport.width, s->viewport.height});
    const auto visible = graphics::intersect_effect_bounds(s->geometry.bounds, clip);
    return visible.width > 0 && visible.height > 0 &&
           services_->interactions().subtree_intersects_input_domain(s->popup);
}

void TooltipComponentHost::synchronize_visibility(runtime::ComponentId id) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    const bool visible = maintenance_allowed(id);
    const bool closed = state->geometry.visible && !visible;
    const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                         services_->motion_preference());
    const bool allowed = state->animated && policy.enabled() && window_active_ && !state->disabled &&
                         !state->boundary_suppressed && services_->components().branch_active(id);
    state->motion.target(visible, services_->animation_time(), policy.tokens().fast, allowed);
    const bool painted = state->motion.painted();
    if (services_->components().set_branch_active(state->popup, painted) |
        services_->components().set_branch_active(state->arrow_component, painted && state->arrow)) {
        services_->mark_scene_structure_dirty();
    }
    if (services_->nodes().set_paint_opacity(services_->components().root(state->popup), state->motion.opacity()) |
        services_->nodes().set_paint_opacity(services_->components().root(state->arrow_component),
                                             state->motion.opacity())) {
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Material);
    }
    if (state->geometry.visible != visible) {
        state->geometry.visible = visible;
        state->needs_measure = visible;
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
    }
    // A fading popup is presentation only. Rich descendants lose input on the closing frame.
    const auto popup_root = state->popup;
    if (!window_active_ || state->disabled || !has_title(*state)) {
        state->deadline.reset();
        state->action_open = false;
        state->context_anchor.reset();
        state->context_press.reset();
    }
    if (closed) {
        // Cleanup callbacks may destroy a component. Snapshot identities and
        // finish every state access before cancelling retained interactions.
        const auto order = services_->interactions().declaration_order();
        const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
        const auto popup = state->popup;
        for (const auto interaction : interactions) {
            const auto* record = services_->interactions().find(interaction);
            if (record && descendant(*services_, record->component, popup) &&
                !services_->interactions().branch_active(interaction)) {
                services_->pointer().cancel_pointer_interaction(interaction);
            }
        }
        services_->focus().synchronize();
    }
    services_->set_input_blocked(popup_root, !visible);
}

void TooltipComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state || !services_->components().scope(id).active() ||
        (open && !services_->interactions().component_active(id))) {
        return;
    }
    state->deadline.reset();
    if (open) {
        state->boundary_suppressed = false;
    }
    if (state->requested == open) {
        synchronize_visibility(id);
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
    if ((changed || previous_request.has_value()) && callback && services_->components().contains(id) &&
        services_->components().scope(id).active()) {
        callback(open);
    }
}

void TooltipComponentHost::observe(runtime::ComponentId id, animation::AnimationTime time) {
    auto* state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
    if (!services_->interactions().component_active(id) && maintenance_allowed(id)) {
        state->hovered = false;
        state->observed = false;
        state->action_open = false;
        state->deadline.reset();
        state->context_anchor.reset();
        state->context_press.reset();
        synchronize_visibility(id);
        return;
    }
    if (!services_->interactions().component_active(id)) {
        const bool requested =
            state->geometry.visible || state->deadline || state->action_open || state->hovered || state->observed;
        state->hovered = state->observed = state->action_open = false;
        state->deadline.reset();
        state->context_anchor.reset();
        state->context_press.reset();
        state->boundary_suppressed = true;
        if (requested) {
            request_open(id, false);
        } else {
            synchronize_visibility(id);
        }
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
        const bool moving = state->motion.running();
        observe(id, time);
        state = services_->components().state<TooltipState>(id);
        if (state && state->geometry.visible != before) {
            ++changed;
        }
        if (state && moving) {
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Material);
            ++changed;
        }
    }
    return changed;
}

void TooltipComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        observe(id, services_->animation_time());
    }
}

std::optional<animation::AnimationTime> TooltipComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* state = services_->components().state<TooltipState>(id);
        if (state && state->deadline && (!result || *state->deadline < *result)) {
            result = state->deadline;
        }
        if (state && state->motion.running()) {
            const auto next = services_->animation_time() + animation::AnimationDuration::microseconds(16667);
            if (!result || next < *result) {
                result = next;
            }
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
                state->context_press.reset();
            }
            request_open(id, false);
        }
        synchronize_visibility(id);
    }
}

void TooltipComponentHost::synchronize_auxiliary_motion() {
    const auto ids = mounted_;
    for (auto id : ids) {
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
            state->context_press.reset();
            request_open(id, false);
            return true;
        }
    }
    return false;
}

void TooltipComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                            std::optional<input::InteractionId> hit,
                                            std::optional<input::InteractionId> origin) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* state = services_->components().state<TooltipState>(id);
        if (!state || state->disabled || !has_title(*state) || !window_active_) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_trigger = record && descendant(*services_, record->component, state->trigger);
        const bool on_wrapper = hit == state->interaction;
        if (event.action == input::PointerAction::down && event.button == input::PointerButton::secondary) {
            state->context_press =
                (in_trigger || on_wrapper) && hit ? std::optional{std::pair{event.pointer, *hit}} : std::nullopt;
        }
        const bool context_release = state->context_press && hit && state->context_press->first == event.pointer &&
                                     state->context_press->second == *hit;
        if (event.action == input::PointerAction::cancel ||
            (event.action == input::PointerAction::up && event.button == input::PointerButton::secondary)) {
            state->context_press.reset();
        }
        const auto bounds = state->geometry.bounds;
        const bool in_popup = state->geometry.visible && event.x >= bounds.x && event.y >= bounds.y &&
                              event.x < bounds.x + bounds.width && event.y < bounds.y + bounds.height;
        state->hovered = event.action != input::PointerAction::cancel && (in_trigger || on_wrapper || in_popup);
        if (!state->triggers.click && !state->triggers.context_menu) {
            continue;
        }
        if (event.action == input::PointerAction::down &&
            (event.button == input::PointerButton::primary || event.button == input::PointerButton::secondary) &&
            !in_trigger && !on_wrapper && !in_popup) {
            if (state->open || state->action_open || state->requested == true) {
                state->action_open = false;
                state->context_anchor.reset();
                state->context_press.reset();
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
            state->context_press.reset();
            request_open(id, next);
        } else if (event.button == input::PointerButton::secondary && state->triggers.context_menu && context_release) {
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
    const auto token = theme.tooltip();
    const runtime::SemanticTypography typography{theme.typography().font_family, theme.typography().font_weight, false,
                                                 token.font_size, token.line_height};
    const auto color_signal = state->foreground;
    const auto font_signal = state->typography;
    color_signal.set({token.text.red(), token.text.green(), token.text.blue(), token.text.alpha()});
    if (!services_->components().contains(id)) {
        return;
    }
    font_signal.set(typography);
    state = services_->components().state<TooltipState>(id);
    if (!state) {
        return;
    }
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
    const auto available = read_prop(props.available_);
    const auto open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    const auto disabled = read_prop(props.disabled_);
    const auto initial_title = read_prop(props.title_);
    const auto placement = read_prop(props.placement_);
    const auto mode = read_prop(props.trigger_);
    const auto triggers = props.triggers_ ? read_prop(*props.triggers_) : trigger_actions(mode);
    const auto arrow = read_prop(props.arrow_);
    const auto point_at_center = read_prop(props.point_at_center_);
    const auto adjust = read_prop(props.adjust_);
    const auto direction = read_prop(props.direction_);
    static_cast<void>(directional_popup_placement(placement, direction));
    const auto animated = read_prop(props.animated_);
    const auto enter = read_prop(props.enter_);
    const auto leave = read_prop(props.leave_);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TooltipState>();

    struct MountRollback final {
        WindowComponentServices& services;
        runtime::ComponentId owner;
        bool committed{};

        ~MountRollback() noexcept {
            if (!committed) {
                services.retire_failed_component(owner);
            }
        }
    } rollback{services, id};

    const auto valid = [&](bool complete = false) {
        const auto* live = services.components().state<TooltipState>(id);
        if (!live || !services.components().scope(id).active()) {
            return false;
        }
        for (const auto child : {live->trigger, live->popup, live->arrow_component}) {
            if (child.valid()
                    ? !services.components().contains(child) || services.components().parent(child) != std::optional{id}
                    : complete) {
                services.retire_failed_component(id);
                return false;
            }
        }
        return true;
    };
    auto& state = build.state<TooltipState>(id);
    state.component = id;
    state.node = build.root(id);
    state.controlled = props.open_.has_value();
    state.rich_title = title != nullptr;
    state.available = available;
    state.open = open;
    state.disabled = disabled;
    state.title.set(initial_title);
    state.placement = placement;
    state.mode = mode;
    state.triggers = triggers;
    state.arrow = arrow;
    state.point_at_center = point_at_center;
    state.adjust = adjust;
    state.direction = direction;
    state.text_direction.set(state.direction);
    state.animated = animated;
    state.enter = enter;
    state.leave = leave;
    state.callback = props.on_open_;
    runtime::connect_layout_style(build.scope(id), props.layout_, state.node, services.nodes(), services.dirty());
    if (!valid()) {
        return;
    }
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
            if (!valid()) {
                return;
            }
            state.popup = children.mount_component<int>(0);
            const auto popup_node = children.root(state.popup);
            services.nodes().require(popup_node).clip_content = true;
            services.nodes().require(popup_node).clip_decorations = false;
            services.layout().set_layout(popup_node, layout::BoxLayout{});
            services.components().set_window_layer(state.popup,
                                                   build.theme_scope()->snapshot().tooltip().z_index_popup);
            const auto popup_interaction =
                services.interactions().create({state.popup, popup_node, {}, true, false, {}, false});
            const auto popup_hit =
                children.register_scene_fragment(state.popup, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(popup_hit, {}, popup_interaction);
            children.on_resource_cleanup(state.popup, [&services, popup = state.popup, popup_hit, popup_interaction] {
                services.interactions().set_input_blocked(popup, false);
                services.pointer().cancel_interaction(popup_interaction);
                services.interactions().remove(popup_interaction);
                services.scene_composer().remove_fragment(popup_hit);
            });
            const auto body =
                children.register_scene_fragment(state.popup, runtime::SceneFragmentPlacement::before_children);
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            graphics::QuadInstance hidden_body;
            hidden_body.opacity = 0;
            state.body =
                services.surfaces().create_surface(state.popup, popup_node, body, std::span{&hidden_body, 1}, effects);
            children.on_resource_cleanup(state.popup, [&services, popup_node, body_id = state.body] {
                services.surfaces().destroy(body_id);
                services.layout().remove_layout(popup_node);
            });
            const auto fill =
                children.register_scene_fragment(state.popup, runtime::SceneFragmentPlacement::before_children);
            state.fill = services.surfaces().create_content_range(fill, {});
            children.on_resource_cleanup(
                state.popup, [&services, fill_id = state.fill] { services.surfaces().destroy_content_range(fill_id); });
            const Content title_content = title ? Content{SlotContentAccess::function(*title)} : Content{[&state] {
                ryn::Text(TextProps{}.content(state.title).direction(state.text_direction));
            }};
            children.mount_slot_with_semantic_text_style(state.popup, title_content,
                                                         Prop<runtime::SemanticForeground>{state.foreground},
                                                         Prop<runtime::SemanticTypography>{state.typography});
            if (!valid()) {
                return;
            }
            state.arrow_component = children.mount_component<int>(0);
            const auto arrow_node = children.root(state.arrow_component);
            services.layout().set_layout(arrow_node, layout::BoxLayout{});
            services.components().set_window_layer(state.arrow_component,
                                                   build.theme_scope()->snapshot().tooltip().z_index_popup);
            children.on_resource_cleanup(state.arrow_component,
                                         [&services, arrow_node] { services.layout().remove_layout(arrow_node); });
        }});
    if (!valid(true)) {
        return;
    }
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
    const auto connect = [this, id]<typename T, typename Apply>(const Prop<T>& prop, Apply apply) {
        if (!services_->components().contains(id) || !services_->components().scope(id).active()) {
            return;
        }
        auto run = [this, id, prop, apply = std::move(apply), last = std::optional<T>{}]() mutable {
            auto value = read_prop(prop);
            if (services_->components().contains(id) && services_->components().scope(id).active()) {
                if (last && *last == value) {
                    return;
                }
                last = value;
                apply(std::move(value));
            }
        };
        if (PropAccess::static_value(prop)) {
            run();
            return;
        }
        auto observer = detail::observe(detail::ObserverPhase::binding, std::move(run), false);
        services_->components().scope(id).on_cleanup([observer] { observer->deactivate(); });
        observer->run();
    };
    connect(props.title_, [this, id](String value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->needs_measure = true;
            const auto title = state->title;
            title.set(std::move(value));
            synchronize_visibility(id);
        }
    });
    connect(props.available_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->available = value;
            state->observed = false;
            synchronize_visibility(id);
        }
    });
    if (props.open_) {
        connect(*props.open_, [this, id](bool value) {
            if (auto* state = services_->components().state<TooltipState>(id)) {
                state->open = value;
                state->requested = value;
                if (services_->interactions().component_active(id)) {
                    state->boundary_suppressed = false;
                }
                synchronize_visibility(id);
            }
        });
    }
    connect(props.disabled_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->disabled = value;
            state->observed = false;
            synchronize_visibility(id);
        }
    });
    connect(props.placement_, [this, id](TooltipPlacement value) {
        validate_placement(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->placement = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect(props.direction_, [this, id](TextDirection value) {
        static_cast<void>(directional_popup_placement(TooltipPlacement::Top, value));
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->direction = value;
            const auto direction = state->text_direction;
            state->needs_measure = true;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Geometry);
            direction.set(value);
        }
    });
    connect(props.animated_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->animated = value;
            synchronize_visibility(id);
        }
    });
    if (!props.triggers_) {
        connect(props.trigger_, [this, id](TooltipTriggerMode value) {
            validate_mode(value);
            if (auto* state = services_->components().state<TooltipState>(id)) {
                if (state->triggers == trigger_actions(value)) {
                    return;
                }
                state->mode = value;
                state->triggers = trigger_actions(value);
                state->action_open = false;
                state->context_anchor.reset();
                state->context_press.reset();
                state->observed = false;
                state->deadline.reset();
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                request_open(id, false);
            }
        });
    }
    if (props.triggers_) {
        connect(*props.triggers_, [this, id](TooltipTriggers value) {
            if (auto* state = services_->components().state<TooltipState>(id)) {
                if (state->triggers == value) {
                    return;
                }
                state->triggers = value;
                state->action_open = false;
                state->context_anchor.reset();
                state->context_press.reset();
                state->observed = false;
                state->deadline.reset();
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                request_open(id, false);
            }
        });
    }
    connect(props.arrow_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->arrow = value;
            services_->components().set_branch_active(state->arrow_component, state->geometry.visible && value);
            services_->mark_scene_structure_dirty();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect(props.point_at_center_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->point_at_center = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect(props.adjust_, [this, id](bool value) {
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->adjust = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect(props.enter_, [this, id](Duration value) {
        validate_delay(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->enter = value;
            state->observed = false;
            state->deadline.reset();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect(props.leave_, [this, id](Duration value) {
        validate_delay(value);
        if (auto* state = services_->components().state<TooltipState>(id)) {
            state->leave = value;
            state->observed = false;
            state->deadline.reset();
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    if (!valid(true)) {
        return;
    }
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
    rollback.committed = valid(true);
}

void TooltipComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    if (mounted_.empty()) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        services_->popover().position_ancestors(id, viewport, clip);
        observe(id, services_->animation_time());
        auto* state = services_->components().state<TooltipState>(id);
        if (!state || !state->geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().tooltip();
        const auto popup = services_->components().root(state->popup);
        const auto effective_clip = services_->component_clip(state->popup, viewport, clip);
        auto& engine = services_->layout();
        const bool measure = state->needs_measure || state->viewport != viewport || state->clip != effective_clip ||
                             services_->nodes().require(popup).measure_generation != engine.generation();
        if (measure) {
            const float width = std::min(token.max_width, effective_clip.width);
            const float horizontal = std::min(token.padding_inline, width / 2);
            const float vertical = std::min(token.padding_block, effective_clip.height / 2);
            engine.set_layout(popup, layout::BoxLayout{{horizontal, vertical, horizontal, vertical}});
            state->popup_size = engine.measure_child(
                popup, {0, width, std::min(token.min_height, effective_clip.height), effective_clip.height});
            state->needs_measure = false;
            state->viewport = viewport;
            state->clip = effective_clip;
        }
        const auto& node = services_->nodes().require(state->node);
        auto anchor = node.bounds;
        anchor.x += node.translation.x;
        anchor.y += node.translation.y;
        if (state->context_anchor) {
            anchor = {state->context_anchor->x, state->context_anchor->y, 0, 0};
        }
        state->geometry = position_tooltip(
            anchor, state->popup_size, effective_clip, directional_popup_placement(state->placement, state->direction),
            token.gap + (state->arrow ? token.arrow_size : 0), state->adjust, state->arrow && state->point_at_center,
            std::max(12.0F, token.border_radius + token.arrow_size));
        const auto& current = services_->nodes().require(popup);
        if (measure || current.bounds != state->geometry.bounds) {
            engine.place_child(popup, state->geometry.bounds);
        }
        const int axis = side(state->geometry.placement);
        const bool horizontal = axis < 2;
        const auto bounds = state->geometry.bounds;
        const auto radius = std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2);
        const auto extent = horizontal ? bounds.width : bounds.height;
        const float size = std::min(token.arrow_size, std::max(0.0F, extent / 2 - radius));
        const bool arrow_visible = state->arrow && size > 0;
        state->arrow_path.reset();
        if (services_->components().set_branch_active(state->arrow_component, arrow_visible)) {
            services_->mark_scene_structure_dirty();
        }
        if (arrow_visible) {
            const auto center = std::clamp(state->geometry.arrow_center, radius + size, extent - radius - size);
            state->geometry.arrow_center = center;
            state->arrow_path = graphics::TooltipArrowPath{static_cast<graphics::TooltipArrowSide>(axis), center, size,
                                                           token.arrow_size};
            const auto arrow_node = services_->components().root(state->arrow_component);
            runtime::Rect arrow_bounds{0, 0, horizontal ? size * 2 : token.arrow_size,
                                       horizontal ? token.arrow_size : size * 2};
            (void)engine.measure_child(
                arrow_node, {arrow_bounds.width, arrow_bounds.width, arrow_bounds.height, arrow_bounds.height});
            if (axis == 0) {
                arrow_bounds.x = bounds.x + center - size;
                arrow_bounds.y = bounds.y + bounds.height;
            } else if (axis == 1) {
                arrow_bounds.x = bounds.x + center - size;
                arrow_bounds.y = bounds.y - token.arrow_size;
            } else if (axis == 2) {
                arrow_bounds.x = bounds.x + bounds.width;
                arrow_bounds.y = bounds.y + center - size;
            } else {
                arrow_bounds.x = bounds.x - token.arrow_size;
                arrow_bounds.y = bounds.y + center - size;
            }
            if (services_->nodes().require(arrow_node).bounds != arrow_bounds || measure) {
                engine.place_child(arrow_node, arrow_bounds);
            }
        }
    }
}

void TooltipComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* state = services_->components().state<TooltipState>(id);
        if (!state || !state->motion.painted()) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().tooltip();
        const auto bounds = state->geometry.bounds;
        const auto clip = state->clip;
        const graphics::LogicalRoundedRect shape{
            bounds, std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2)};
        const auto fill =
            state->arrow_path
                ? graphics::make_tooltip_fill_effect(shape, *state->arrow_path, token.background, 1, {},
                                                     graphics::EffectClip{1, clip})
                : graphics::make_shadow_effect(shape, ShadowLayer{ShadowKind::outer, {}, 0, 0, token.background}, {},
                                               graphics::EffectClip{1, clip});
        (void)services_->surfaces().update_content_effects(state->fill, std::span{&fill, 1});
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
