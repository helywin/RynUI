#include "component/popconfirm_component.hpp"
#include "component/config_defaults.hpp"
#include "component/popup_motion.hpp"
#include "component/localized_prop.hpp"
#include "component/confirm_action.hpp"
#include "component/popconfirm_model.hpp"
#include "component/select_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local PopconfirmComponentHost* active_popconfirms{};

struct PopconfirmState final {
    PopconfirmState(bool controlled, bool open) : model(controlled, open) {}

    PopconfirmModel model;
    PopconfirmSnapshot snapshot;
    runtime::NodeId node;
    runtime::ComponentId message;
    runtime::ComponentId body;
    runtime::ComponentId title_component;
    runtime::ComponentId footer;
    runtime::ComponentId arrow_component;
    input::InteractionId trigger_hit;
    bool native_trigger{};
    input::InteractionId popup_hit;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId fill;
    std::optional<graphics::TooltipArrowPath> arrow_path;
    Signal<String> title{String{}};
    Signal<String> description{String{}};
    Signal<String> ok_text{String{u8"OK"}};
    Signal<String> cancel_text{String{u8"Cancel"}};
    std::uint64_t label_revision{};
    std::uint64_t refresh_revision{};
    Signal<bool> loading{false};
    bool user_loading{};
    bool action_starting{};
    ConfirmAction action_source;
    std::function<bool()> action_live;
    std::shared_ptr<ConfirmActionSession> action;
    std::function<void(ConfirmActionError)> action_error;
    std::shared_ptr<PopconfirmAnchorBinding> reference;
    std::shared_ptr<PopconfirmAnchorBinding> service_anchor;
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> warning_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    bool rich_title{};
    bool rich_description{};
    bool disabled{};
    bool show_cancel{true};
    bool arrow{true};
    bool point_at_center{};
    bool adjust{true};
    bool animated{};
    PopupMotion motion;
    TextDirection direction{TextDirection::LeftToRight};
    Signal<TextDirection> text_direction{TextDirection::LeftToRight};
    PopconfirmTriggers triggers{false, false, true, false};
    Duration enter{Duration::milliseconds(100)};
    Duration leave{Duration::milliseconds(100)};
    bool hovered{};
    bool latched{};
    bool dismissed{};
    bool observed_desire{};
    bool pending_open{};
    std::optional<bool> press_open;
    std::optional<animation::AnimationTime> trigger_deadline;
    std::optional<runtime::Point> context_anchor;
    std::optional<std::pair<input::PointerIdentity, input::InteractionId>> context_press;
    std::optional<bool> completion;
    std::function<void(bool)> after_open;
    theme_runtime::Subscription motion_subscription;
    bool needs_measure{true};
    bool ready{};
    bool focus_pending{};
    bool restore{true};
    PopconfirmPlacement placement{PopconfirmPlacement::Top};
    runtime::Size viewport;
    runtime::Size popup_size;
    std::optional<input::InteractionId> restore_target;
    std::function<void(bool)> on_open;
    std::function<void()> on_confirm;
    std::function<void()> on_cancel;
};

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

bool can_focus(WindowComponentServices& services, input::InteractionId id) {
    const auto* record = services.interactions().find(id);
    return record && record->eligible && record->focusable && services.interactions().branch_active(id);
}

std::optional<input::InteractionId> first_focus(WindowComponentServices& services, runtime::ComponentId root) {
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && can_focus(services, hit) && descendant(services, record->component, root)) {
            return hit;
        }
    }
    return {};
}

bool button_can_activate(WindowComponentServices& services, runtime::ComponentId button) {
    const auto hit = first_focus(services, button);
    const auto* record = hit ? services.interactions().find(*hit) : nullptr;
    const auto handlers = record ? record->focus_handlers : nullptr;
    return record && (!handlers || !handlers->activation_allowed || handlers->activation_allowed());
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

void validate_placement(PopconfirmPlacement placement) {
    if (placement < PopconfirmPlacement::Top || placement > PopconfirmPlacement::RightBottom) {
        throw std::invalid_argument("Popconfirm placement is invalid");
    }
}

PopconfirmTriggers trigger_actions(PopconfirmTriggerMode mode) {
    if (mode < PopconfirmTriggerMode::Manual || mode > PopconfirmTriggerMode::ContextMenu) {
        throw std::invalid_argument("Popconfirm trigger is invalid");
    }
    return {mode == PopconfirmTriggerMode::Hover || mode == PopconfirmTriggerMode::HoverFocus,
            mode == PopconfirmTriggerMode::Focus || mode == PopconfirmTriggerMode::HoverFocus,
            mode == PopconfirmTriggerMode::Click, mode == PopconfirmTriggerMode::ContextMenu};
}

void validate_delay(Duration delay) {
    if (delay.count_milliseconds() > 86400000) {
        throw std::invalid_argument("Popconfirm delay cannot exceed one day");
    }
}

bool trigger_can_activate(WindowComponentServices& services, runtime::ComponentId owner,
                          std::optional<input::InteractionId> hit) {
    const auto* state = services.components().state<PopconfirmState>(owner);
    const auto* record = hit ? services.interactions().find(*hit) : nullptr;
    if (!state || !record || !record->eligible || !services.interactions().branch_active(*hit)) {
        return false;
    }
    if (*hit == state->trigger_hit) {
        return !state->native_trigger;
    }
    for (const auto candidate : services.interactions().declaration_order()) {
        const auto* control = services.interactions().find(candidate);
        if (control && control->focusable && control->eligible && services.interactions().branch_active(candidate) &&
            descendant(services, control->component, state->snapshot.trigger) &&
            descendant(services, record->component, control->component)) {
            const auto handlers = control->focus_handlers;
            const bool allowed = !handlers || !handlers->activation_allowed || handlers->activation_allowed();
            const auto* live = services.interactions().find(candidate);
            state = services.components().state<PopconfirmState>(owner);
            record = services.interactions().find(*hit);
            return allowed && state && live && record && live->eligible &&
                   services.interactions().branch_active(candidate) &&
                   descendant(services, live->component, state->snapshot.trigger) &&
                   descendant(services, record->component, live->component);
        }
    }
    return false;
}

runtime::ComponentId group(WindowComponentServices& services, layout::LayoutModel model) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, std::move(model));
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}
} // namespace

bool popconfirm_anchor_live(const PopconfirmAnchorBinding& binding) {
    if (binding.thread != std::this_thread::get_id() || !binding.alive || binding.context.owner.expired() ||
        !binding.context.config || !binding.context.config->active()) {
        return false;
    }
    auto& host = binding.services->components();
    return host.active() && host.contains(binding.owner) && host.scope(binding.owner).active();
}

std::function<void()> PopconfirmComponentHost::detach_action(runtime::ComponentId id) noexcept {
    auto* state = services_->components().state<PopconfirmState>(id);
    return state && state->action ? state->action->detach_cancel() : ConfirmAction::Cancel{};
}

std::size_t PopconfirmComponentHost::tick_auxiliary(animation::AnimationTime time) {
    time = clock_.observe(time).effective;
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    std::size_t count{};
    for (const auto id : ids) {
        const auto* state = services_->components().state<PopconfirmState>(id);
        const auto action = state ? state->action : nullptr;
        if (action && action->tick(time)) {
            ++count;
        }
        if (life.expired()) {
            break;
        }
        auto* live = services_->components().state<PopconfirmState>(id);
        const bool moving = live && live->motion.running();
        observe(id, time);
        if (life.expired()) {
            break;
        }
        if (moving && services_->components().contains(id)) {
            refresh(id);
            ++count;
        }
    }
    return count;
}

std::optional<animation::AnimationTime> PopconfirmComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto id : mounted_) {
        const auto* state = services_->components().state<PopconfirmState>(id);
        const auto deadline = state && state->action ? state->action->deadline() : std::nullopt;
        if (deadline && (!result || *deadline < *result)) {
            result = deadline;
        }
        if (state && state->trigger_deadline && (!result || *state->trigger_deadline < *result)) {
            result = state->trigger_deadline;
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

void PopconfirmComponentHost::observe(runtime::ComponentId id, animation::AnimationTime time) {
    time = clock_.observe(time).effective;
    auto* state = services_->components().state<PopconfirmState>(id);
    if (!state || state->disabled || !window_active_ || !services_->interactions().component_active(id) ||
        state->service_anchor ||
        (state->reference && (state->reference->service_only || !state->reference->service.expired()))) {
        return;
    }
    if (!state->triggers.hover && !state->triggers.focus) {
        state->trigger_deadline.reset();
        return;
    }
    const auto focus = services_->focus().state().focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    const bool trigger_focus =
        record && (focus == state->trigger_hit || descendant(*services_, record->component, state->snapshot.trigger));
    const bool popup_focus =
        record && state->snapshot.geometry.visible && descendant(*services_, record->component, state->snapshot.popup);
    const bool desire = state->latched || (state->triggers.hover && state->hovered) ||
                        (state->triggers.focus && trigger_focus) ||
                        ((state->triggers.hover || state->triggers.focus) && popup_focus);
    if (!desire) {
        state->dismissed = false;
    }
    if (state->action_starting || (state->action && state->action->pending())) {
        state->trigger_deadline.reset();
        return;
    }
    if (desire != state->observed_desire) {
        state->observed_desire = desire;
        state->trigger_deadline.reset();
        if (!state->dismissed && desire != state->snapshot.geometry.visible) {
            state->pending_open = desire;
            const auto delay = desire && state->triggers.focus && trigger_focus ? Duration{}
                               : desire                                         ? state->enter
                                                                                : state->leave;
            state->trigger_deadline = time + animation::AnimationDuration::microseconds(
                                                 static_cast<std::int64_t>(delay.count_milliseconds() * 1000));
            services_->request_frame();
        }
    }
    if (state->trigger_deadline && time >= *state->trigger_deadline) {
        const bool open = state->pending_open;
        state->trigger_deadline.reset();
        request_open(id, open);
    }
}

void PopconfirmComponentHost::synchronize_auxiliary_motion() {
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    for (const auto id : ids) {
        refresh(id);
        if (life.expired()) {
            return;
        }
    }
}

bool PopconfirmComponentHost::presentation_enabled(runtime::ComponentId id) const {
    const auto* state = services_->components().state<PopconfirmState>(id);
    return state && state->animated;
}

void PopconfirmComponentHost::arm_service_close(runtime::ComponentId id) {
    auto* state = services_->components().state<PopconfirmState>(id);
    if (!state || !services_->components().scope(id).active()) {
        throw std::logic_error("Popconfirm service owner is retired");
    }
    state->completion = false;
    refresh(id);
    services_->request_frame();
}

bool PopconfirmComponentHost::synchronize_auxiliary_fragments() {
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    for (const auto id : ids) {
        auto* state = services_->components().state<PopconfirmState>(id);
        if (!state || !state->completion || state->motion.running() ||
            state->snapshot.geometry.visible != *state->completion) {
            continue;
        }
        const bool visible = *state->completion;
        state->completion.reset();
        const auto callback = state->after_open;
        if (callback) {
            callback(visible);
            if (life.expired()) {
                return false;
            }
        }
    }
    return false;
}

PopconfirmComponentHost::PopconfirmComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

PopconfirmComponentHost::~PopconfirmComponentHost() {
    lifetime_.reset();
    services_->detach(*this);
}

void* PopconfirmComponentHost::begin_mount() noexcept {
    auto* previous = active_popconfirms;
    active_popconfirms = this;
    return previous;
}

void PopconfirmComponentHost::end_mount(void* previous) noexcept {
    active_popconfirms = static_cast<PopconfirmComponentHost*>(previous);
}

void PopconfirmComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void PopconfirmComponentHost::on_dispose() noexcept {
    lifetime_.reset();
    mounted_.clear();
}

PopconfirmSnapshot PopconfirmComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<PopconfirmState>(id);
    if (!state) {
        throw std::out_of_range("Popconfirm component is stale");
    }
    return state->snapshot;
}

void PopconfirmComponentHost::refresh(runtime::ComponentId id) {
    const auto life = std::weak_ptr{lifetime_};
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    const bool visible =
        s->model.open() && !s->disabled && window_active_ && services_->interactions().component_active(id);
    if (s->reference) {
        s->reference->open = s->model.open();
    }
    // Input-boundary publication synchronously revisits this owner. A visit
    // with the same effective visibility must not supersede the transition
    // that still owns focus transfer/restoration below.
    const auto revision = visible != s->snapshot.geometry.visible ? ++s->refresh_revision : s->refresh_revision;
    if (!visible && s->action && s->action->pending()) {
        const auto life = std::weak_ptr{lifetime_};
        auto cancel = s->action->detach_cancel();
        const auto loading = s->loading;
        loading.set(s->user_loading);
        ConfirmActionSession::run_cancel(std::move(cancel));
        if (life.expired()) {
            return;
        }
        s = services_->components().state<PopconfirmState>(id);
        if (!s || s->refresh_revision != revision) {
            return;
        }
    }
    const bool changed = visible != s->snapshot.geometry.visible;
    s->snapshot.geometry.visible = visible;
    if (changed) {
        s->completion = visible;
    }
    const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                         services_->motion_preference());
    const bool allowed = s->animated && policy.enabled() && window_active_ && !s->disabled &&
                         services_->components().branch_active(id) &&
                         services_->interactions().component_lifetime_active(id) &&
                         (visible || services_->interactions().component_active(id));
    s->motion.target(visible, services_->animation_time(), policy.tokens().fast, allowed);
    const bool painted = s->motion.painted();
    s->snapshot.paint_visible = painted;
    s->snapshot.transitioning = s->motion.running();
    s->snapshot.opacity = s->motion.opacity();
    const bool description = s->rich_description || !s->description.get().empty();
    const bool title = s->rich_title || !s->title.get().empty();
    bool structure = services_->components().set_branch_active(s->snapshot.popup, painted);
    structure |= services_->components().set_branch_active(s->arrow_component, painted && s->arrow);
    if (services_->nodes().set_paint_opacity(services_->components().root(s->snapshot.popup), s->motion.opacity()) |
        services_->nodes().set_paint_opacity(services_->components().root(s->arrow_component), s->motion.opacity())) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    structure |= services_->components().set_branch_active(s->snapshot.cancel, s->show_cancel);
    services_->nodes().require(services_->components().root(s->snapshot.cancel)).out_of_flow = !s->show_cancel;
    structure |= services_->components().set_branch_active(s->snapshot.description, description);
    structure |= services_->components().set_branch_active(s->title_component, title);
    services_->nodes().require(services_->components().root(s->snapshot.description)).out_of_flow = !description;
    services_->nodes().require(services_->components().root(s->title_component)).out_of_flow = !title;
    services_->interactions().set_eligible(s->trigger_hit, !s->disabled);
    if (structure) {
        s->needs_measure = true;
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::HitTest);
    }
    const auto focused_before_block = services_->focus().state().focused;
    const auto* focused_record = focused_before_block ? services_->interactions().find(*focused_before_block) : nullptr;
    const bool restore_focus = changed && !visible && s->restore && focused_record &&
                               descendant(*services_, focused_record->component, s->snapshot.popup);
    const auto popup_root = s->snapshot.popup;
    // A parent Modal has not published its input authority while its content
    // slot is mounting. Do not force a registry reconciliation from that slot.
    if (s->ready) {
        services_->set_input_blocked(popup_root, !visible);
    }
    if (life.expired()) {
        return;
    }
    s = services_->components().state<PopconfirmState>(id);
    if (!s || s->refresh_revision != revision) {
        return;
    }
    if (!changed) {
        return;
    }
    if (visible) {
        const auto trigger = s->service_anchor ? s->service_anchor->trigger : s->snapshot.trigger;
        const auto focus = services_->focus().state().focused;
        const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
        s->restore_target =
            record && descendant(*services_, record->component, trigger) ? focus : first_focus(*services_, trigger);
        if (!s->restore_target && s->service_anchor) {
            const auto* anchor = services_->components().state<PopconfirmState>(s->service_anchor->owner);
            if (anchor && can_focus(*services_, anchor->trigger_hit)) {
                s->restore_target = anchor->trigger_hit;
            }
        }
        if (!s->restore_target && can_focus(*services_, s->trigger_hit)) {
            s->restore_target = s->trigger_hit;
        }
        const bool keyboard_trigger = s->triggers.focus && services_->focus().state().focus_visible && record &&
                                      descendant(*services_, record->component, trigger);
        s->focus_pending = !s->triggers.hover || s->latched || keyboard_trigger;
        return;
    }
    s->focus_pending = false;
    const auto popup = s->snapshot.popup;
    const auto target = s->restore_target;
    s->restore_target.reset();
    const auto order = services_->interactions().declaration_order();
    const std::vector<input::InteractionId> hits{order.begin(), order.end()};
    for (auto hit : hits) {
        const auto* live = services_->interactions().find(hit);
        if (live && descendant(*services_, live->component, popup)) {
            services_->pointer().cancel_pointer_interaction(hit);
        }
    }
    // Descendant layers share logical ownership even when positioned outside
    // this popup. Suppress their controlled state before a later parent reopen.
    services_->select().on_input_boundary_changed();
    services_->tooltip().on_input_boundary_changed();
    if (restore_focus && target && can_focus(*services_, *target)) {
        services_->focus().defer_focus(target, input::FocusModality::keyboard);
    } else {
        services_->focus().synchronize();
    }
}

void PopconfirmComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !services_->components().scope(id).active() || s->action_starting ||
        (s->action && s->action->pending()) ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    s->restore = restore;
    s->trigger_deadline.reset();
    if (!open) {
        s->latched = false;
        s->dismissed = true;
        s->context_press.reset();
    }
    const bool notify = s->model.request(open);
    const auto callback = s->on_open;
    const auto life = std::weak_ptr{lifetime_};
    refresh(id);
    if (!life.expired() && notify && callback && services_->components().contains(id) &&
        services_->components().scope(id).active()) {
        callback(open);
    }
}

void PopconfirmComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    s->restore = false;
    s->hovered = false;
    s->latched = false;
    s->observed_desire = false;
    s->dismissed = true;
    s->trigger_deadline.reset();
    s->context_press.reset();
    s->context_anchor.reset();
    s->press_open.reset();
    auto cancel = s->action ? s->action->detach_cancel() : ConfirmAction::Cancel{};
    const bool notify = s->model.suppress();
    const auto callback = s->on_open;
    const auto life = std::weak_ptr{lifetime_};
    s->loading.set(s->user_loading);
    ConfirmActionSession::run_cancel(std::move(cancel));
    if (life.expired()) {
        return;
    }
    refresh(id);
    if (!life.expired() && notify && callback && services_->components().contains(id) &&
        services_->components().scope(id).active()) {
        callback(false);
    }
}

void PopconfirmComponentHost::confirm(runtime::ComponentId id) {
    const auto life = std::weak_ptr{lifetime_};
    const auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !s->snapshot.geometry.visible || s->disabled || s->loading.get() ||
        (s->action_live && !s->action_live()) || !services_->interactions().component_active(id)) {
        return;
    }
    const auto callback = s->on_confirm;
    if (s->action_source) {
        const auto session = s->action;
        const auto source = s->action_source;
        services_->components().state<PopconfirmState>(id)->action_starting = true;
        const auto loading = s->loading;
        loading.set(true);
        if (life.expired()) {
            return;
        }
        auto* state = services_->components().state<PopconfirmState>(id);
        if (state && services_->components().scope(id).active()) {
            state->action_starting = false;
            if (state->snapshot.geometry.visible && !state->disabled && window_active_) {
                services_->request_frame();
                session->start(source, services_->animation_time());
            } else {
                loading.set(state->user_loading);
            }
        }
        return;
    }
    if (callback) {
        callback();
    }
    if (life.expired()) {
        return;
    }
    s = services_->components().state<PopconfirmState>(id);
    if (s && services_->components().scope(id).active() && !s->loading.get()) {
        request_open(id, false);
    }
}

void PopconfirmComponentHost::cancel(runtime::ComponentId id) {
    const auto life = std::weak_ptr{lifetime_};
    const auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !s->snapshot.geometry.visible || !s->show_cancel || s->action_starting ||
        (s->action && s->action->pending()) || !services_->interactions().component_active(id)) {
        return;
    }
    const auto callback = s->on_cancel;
    request_open(id, false);
    if (!life.expired() && callback && services_->components().contains(id) &&
        services_->components().scope(id).active()) {
        callback();
    }
}

void PopconfirmComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
    s->text_color.set(foreground(token.text));
    s->title_color.set(foreground(token.title));
    s->warning_color.set(foreground(token.warning));
    const bool description = s->rich_description || !s->description.get().empty();
    s->font.set({token.font_family, token.font_weight, false, token.font_size, token.line_height});
    s->title_font.set({token.font_family, description ? token.title_font_weight : token.font_weight, false,
                       token.font_size, token.line_height});
    if (geometry) {
        s->needs_measure = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure);
    }
    if (services_->components().set_window_layer(s->snapshot.popup, token.z_index_popup) |
        services_->components().set_window_layer(s->arrow_component, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
}

void PopconfirmComponentHost::mount(const PopconfirmProps& props, const PopconfirmTrigger& trigger,
                                    const PopconfirmSlots& slots) {
    if (props.service_anchor_only_ && (props.open_ || props.default_open_)) {
        throw std::invalid_argument("Popconfirm serviceAnchor cannot control declarative open");
    }
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("Popconfirm cannot combine open and defaultOpen");
    }
    if ((slots.title && props.explicit_title_) || (slots.description && props.explicit_description_)) {
        throw std::invalid_argument("Popconfirm text and slot declarations conflict");
    }
    validate_placement(read_prop(props.placement_));
    const auto actions = props.triggers_ ? read_prop(*props.triggers_) : trigger_actions(read_prop(props.trigger_));
    validate_delay(read_prop(props.enter_));
    validate_delay(read_prop(props.leave_));
    const auto direction = config_direction(props.direction_, props.direction_set_);
    static_cast<void>(directional_popup_placement(PopconfirmPlacement::Top, read_prop(direction)));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto locale =
        localized_prop(props.locale_, build.config_scope(), [](const LocaleData& data) { return data.common; });
    const auto ok_text = props.ok_text_ ? *props.ok_text_ : bind([locale] { return read_prop(locale).confirm; });
    const auto cancel_text =
        props.cancel_text_ ? *props.cancel_text_ : bind([locale] { return read_prop(locale).cancel; });
    const auto id = build.mount_component<PopconfirmState>(
        props.open_.has_value(), props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false));
    const auto retained = build.host().retain_state<PopconfirmState>(id);
    auto& s = *retained;
    s.node = build.root(id);
    s.rich_title = slots.title.has_value();
    s.rich_description = slots.description.has_value();
    s.disabled = read_prop(props.disabled_);
    s.show_cancel = read_prop(props.show_cancel_);
    s.triggers = actions;
    s.animated = read_prop(props.animated_);
    s.enter = read_prop(props.enter_);
    s.leave = read_prop(props.leave_);
    s.after_open = props.after_open_;
    s.direction = read_prop(direction);
    s.text_direction.set(s.direction);
    s.motion.target(false, services.animation_time(), {}, false);
    s.on_open = props.on_open_;
    s.on_confirm = props.on_confirm_;
    s.action_source = props.action_;
    s.action_live = props.action_live_;
    s.action_error = props.action_error_;
    s.service_anchor = props.service_anchor_;
    s.action = std::make_shared<ConfirmActionSession>([this, life = std::weak_ptr{lifetime_},
                                                       id](ConfirmActionResult result) {
        if (life.expired()) {
            return;
        }
        auto* state = services_->components().state<PopconfirmState>(id);
        if (!state || !services_->components().scope(id).active() || (state->action_live && !state->action_live())) {
            return;
        }
        const auto loading = state->loading;
        const auto error = state->action_error;
        const auto accepted = state->on_confirm;
        const bool service = bool(state->service_anchor);
        const auto generation = state->action->generation();
        loading.set(state->user_loading);
        if (life.expired()) {
            return;
        }
        state = services_->components().state<PopconfirmState>(id);
        if (!state || !services_->components().scope(id).active() || state->action->generation() != generation ||
            (state->action_live && !state->action_live())) {
            return;
        }
        if (result.error) {
            if (error) {
                error(std::move(*result.error));
            }
        } else if (result.decision == ConfirmActionDecision::Close) {
            if (accepted) {
                accepted();
            }
            if (!life.expired() && !service) {
                request_open(id, false);
            }
        }
    });
    build.scope(id).on_cleanup([action = s.action] { action->cancel(); });
    if (props.reference_) {
        if (const auto previous = props.reference_->binding.lock(); previous && previous->alive) {
            throw std::invalid_argument("PopconfirmRef is already bound");
        }
        s.reference = std::make_shared<PopconfirmAnchorBinding>();
        s.reference->services = &services;
        s.reference->owner = id;
        s.reference->context = build.host().capture_context(id);
        s.reference->controlled = props.open_.has_value();
        s.reference->service_only = props.service_anchor_only_;
        props.reference_->binding = s.reference;
        props.reference_->assigned = true;
        build.scope(id).on_cleanup([reference = s.reference] { reference->alive = false; });
    }
    s.on_cancel = props.on_cancel_;
    s.title.set(read_prop(props.title_));
    s.description.set(read_prop(props.description_));
    s.trigger_hit = services.interactions().create({id, s.node, {}, !s.disabled, true, {}, true, true});
    const auto hit_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit_fragment, {}, s.trigger_hit);
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.trigger_hit, hit_fragment] {
        services.pointer().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.scene_composer().remove_fragment(hit_fragment);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            s.snapshot.trigger = group(services, layout::BoxLayout{});
            children.mount_slot(s.snapshot.trigger, trigger);
            for (const auto hit : services.interactions().declaration_order()) {
                const auto* record = services.interactions().find(hit);
                if (record && record->focusable && descendant(services, record->component, s.snapshot.trigger)) {
                    s.native_trigger = true;
                    break;
                }
            }
            services.interactions().set_tab_stop(s.trigger_hit, !s.native_trigger);
            s.snapshot.popup = group(services, layout::BoxLayout{});
            const auto popup_node = children.root(s.snapshot.popup);
            services.components().set_window_layer(s.snapshot.popup, 1060);
            services.components().set_branch_active(s.snapshot.popup, false);
            services.nodes().require(popup_node).clip_content = true;
            services.nodes().require(popup_node).clip_decorations = false;
            s.popup_hit =
                services.interactions().create({s.snapshot.popup, popup_node, {}, true, false, {}, false, false});
            const auto surface =
                children.register_scene_fragment(s.snapshot.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.surface = services.surfaces().create(s.snapshot.popup, popup_node, surface, s.popup_hit,
                                                   std::span{&initial, 1}, effects);
            children.on_resource_cleanup(s.snapshot.popup, [&services, hit = s.popup_hit, surface = s.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            const auto fill =
                children.register_scene_fragment(s.snapshot.popup, runtime::SceneFragmentPlacement::before_children);
            s.fill = services.surfaces().create_content_range(fill, {});
            children.on_resource_cleanup(s.snapshot.popup, [&services, fill_id = s.fill] {
                services.surfaces().destroy_content_range(fill_id);
            });
            children.mount_slot(
                s.snapshot.popup, Content{[&] {
                    auto& popup = runtime::require_component_build_context();
                    s.message = group(services, layout::FlexLayout{});
                    popup.mount_slot(
                        s.message, Content{[&] {
                            auto& message = runtime::require_component_build_context();
                            const auto icon = group(services, layout::BoxLayout{});
                            services.nodes().require(services.components().root(icon)).external_layout.flex_shrink = 0;
                            message.mount_slot_with_semantic_text_style(
                                icon, Content{[] { Icon(IconProps{}.name(IconName::ExclamationCircleFilled)); }},
                                s.warning_color, s.font);
                            s.body = group(services, layout::FlexLayout{});
                            message.mount_slot(
                                s.body, Content{[&] {
                                    auto& body = runtime::require_component_build_context();
                                    s.title_component = group(services, layout::BoxLayout{});
                                    body.mount_slot_with_semantic_text_style(
                                        s.title_component,
                                        slots.title
                                            ? Content{SlotContentAccess::function(*slots.title)}
                                            : Content{[&s] {
                                                  Text(TextProps{}.content(s.title).direction(s.text_direction));
                                              }},
                                        s.title_color, s.title_font);
                                    s.snapshot.description = group(services, layout::BoxLayout{});
                                    body.mount_slot_with_semantic_text_style(
                                        s.snapshot.description,
                                        slots.description
                                            ? Content{SlotContentAccess::function(*slots.description)}
                                            : Content{[&s] {
                                                  Text(TextProps{}.content(s.description).direction(s.text_direction));
                                              }},
                                        s.text_color, s.font);
                                }});
                        }});
                    s.footer = group(services, layout::FlexLayout{});
                    popup.mount_slot(
                        s.footer, Content{[&] {
                            auto cancel_props = props.cancel_button_.value_or(ButtonProps{}.size(ControlSize::Small));
                            const auto cancel_click = cancel_props.on_click_;
                            cancel_props.onClick([this, id, cancel_click, life = std::weak_ptr{lifetime_}] {
                                if (cancel_click) {
                                    cancel_click();
                                }
                                if (!life.expired()) {
                                    const auto* state = services_->components().state<PopconfirmState>(id);
                                    if (state && button_can_activate(*services_, state->snapshot.cancel)) {
                                        cancel(id);
                                    }
                                }
                            });
                            Button(cancel_props, [&s] { Text(TextProps{}.content(s.cancel_text)); });
                            s.snapshot.cancel = services.components().children(s.footer).back();
                            auto ok_props = props.ok_button_.value_or(
                                ButtonProps{}.type(ButtonType::Primary).size(ControlSize::Small));
                            const auto ok_click = ok_props.on_click_;
                            ok_props
                                .loading(bind([loading = ok_props.loading_, pending = s.loading] {
                                    return read_prop(loading) || pending.get();
                                }))
                                .onClick([this, id, ok_click, life = std::weak_ptr{lifetime_}] {
                                    if (ok_click) {
                                        ok_click();
                                    }
                                    if (!life.expired()) {
                                        const auto* state = services_->components().state<PopconfirmState>(id);
                                        if (state && button_can_activate(*services_, state->snapshot.confirm)) {
                                            confirm(id);
                                        }
                                    }
                                });
                            Button(ok_props, [&s] { Text(TextProps{}.content(s.ok_text)); });
                            s.snapshot.confirm = services.components().children(s.footer).back();
                        }});
                }});
            services.layout().set_layout(
                popup_node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& state = *services_->components().state<PopconfirmState>(id);
                        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
                        const float padding = std::min(token.padding, limits.max_width / 2);
                        const float width = std::max(0.0F, limits.max_width - padding * 2);
                        const auto message = engine.measure_child(services_->components().root(state.message),
                                                                  {0, width, 0, limits.max_height});
                        const auto footer = engine.measure_child(services_->components().root(state.footer),
                                                                 {0, width, 0, limits.max_height});
                        return limits.constrain({std::max(message.width, footer.width) + padding * 2,
                                                 message.height + footer.height + token.message_gap + padding * 2});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto& state = *services_->components().state<PopconfirmState>(id);
                        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
                        const float padding = std::min(token.padding, std::min(rect.width, rect.height) / 2);
                        const float width = std::max(0.0F, rect.width - padding * 2);
                        const auto footer_node = services_->components().root(state.footer);
                        const auto message_node = services_->components().root(state.message);
                        const auto footer = engine.measure_child(footer_node, {0, width, 0, rect.height});
                        const float footer_height = std::min(footer.height, rect.height - padding * 2);
                        const float message_height =
                            std::max(0.0F, rect.height - padding * 2 - footer_height - token.message_gap);
                        static_cast<void>(engine.measure_child(message_node, {width, width, 0, message_height}));
                        engine.place_child(message_node, {rect.x + padding, rect.y + padding, width, message_height});
                        const float footer_x = state.direction == TextDirection::RightToLeft
                                                   ? rect.x + padding
                                                   : rect.x + rect.width - padding - footer.width;
                        engine.place_child(footer_node, {footer_x, rect.y + rect.height - padding - footer_height,
                                                         footer.width, footer_height});
                    }});
            s.arrow_component = group(services, layout::BoxLayout{});
            services.components().set_window_layer(s.arrow_component, 1060);
            services.components().set_branch_active(s.arrow_component, false);
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        const auto* state = services_->components().state<PopconfirmState>(id);
                        return engine.measure_child(services_->components().root(state->snapshot.trigger), limits);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto* state = services_->components().state<PopconfirmState>(id);
                        engine.place_child(services_->components().root(state->snapshot.trigger), rect);
                    }});
    mounted_.push_back(id);
    if (s.reference) {
        s.reference->trigger = s.snapshot.trigger;
    }
    if (s.service_anchor) {
        services.nodes().require(s.node).out_of_flow = true;
        services.interactions().set_focusable(s.trigger_hit, false);
        services.interactions().set_tab_stop(s.trigger_hit, false);
    }
    auto& scope = build.scope(id);
    const auto text = [&](const Prop<String>& prop, Signal<String> PopconfirmState::* field) {
        connect_prop(scope, prop, [this, id, field](String value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                const auto revision = ++state->label_revision;
                const auto target = state->*field;
                target.set(std::move(value));
                state = services_->components().state<PopconfirmState>(id);
                if (!state || state->label_revision != revision) {
                    return;
                }
                state->needs_measure = true;
                update_theme(id, true);
                state = services_->components().state<PopconfirmState>(id);
                if (state && state->label_revision == revision) {
                    refresh(id);
                }
            }
        });
    };
    text(props.title_, &PopconfirmState::title);
    text(props.description_, &PopconfirmState::description);
    text(ok_text, &PopconfirmState::ok_text);
    text(cancel_text, &PopconfirmState::cancel_text);
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                state->model.reconcile(value);
                state->restore = true;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->disabled = value;
            if (value) {
                suppress(id);
            } else {
                refresh(id);
            }
        }
    });
    connect_prop(scope, props.loading_, [this, id](bool value) {
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->user_loading = value;
            state->loading.set(value || state->action_starting || state->action->pending());
        }
    });
    const auto flag = [&](const Prop<bool>& prop, bool PopconfirmState::* field) {
        connect_prop(scope, prop, [this, id, field](bool value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                state->*field = value;
                state->needs_measure = true;
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                refresh(id);
            }
        });
    };
    flag(props.show_cancel_, &PopconfirmState::show_cancel);
    flag(props.arrow_, &PopconfirmState::arrow);
    flag(props.point_at_center_, &PopconfirmState::point_at_center);
    flag(props.adjust_, &PopconfirmState::adjust);
    flag(props.animated_, &PopconfirmState::animated);
    const auto change_actions = [this, id](PopconfirmTriggers value) {
        if (auto* state = services_->components().state<PopconfirmState>(id); state && state->triggers != value) {
            state->triggers = value;
            state->hovered = false;
            state->latched = false;
            state->observed_desire = false;
            state->trigger_deadline.reset();
            state->context_anchor.reset();
            state->context_press.reset();
            state->press_open.reset();
            const auto life = std::weak_ptr{lifetime_};
            request_open(id, false);
            if (life.expired()) {
                return;
            }
            if (auto* live = services_->components().state<PopconfirmState>(id); live && live->triggers == value) {
                live->dismissed = false;
            }
        }
    };
    if (props.triggers_) {
        connect_prop(scope, *props.triggers_, change_actions);
    } else {
        connect_prop(scope, props.trigger_,
                     [change_actions](PopconfirmTriggerMode value) { change_actions(trigger_actions(value)); });
    }
    const auto delay = [&](const Prop<Duration>& prop, Duration PopconfirmState::* field) {
        connect_prop(scope, prop, [this, id, field](Duration value) {
            validate_delay(value);
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                state->*field = value;
                if (state->trigger_deadline) {
                    const auto next = state->pending_open ? state->enter : state->leave;
                    state->trigger_deadline =
                        services_->animation_time() + animation::AnimationDuration::microseconds(
                                                          static_cast<std::int64_t>(next.count_milliseconds() * 1000));
                    services_->request_frame();
                }
            }
        });
    };
    delay(props.enter_, &PopconfirmState::enter);
    delay(props.leave_, &PopconfirmState::leave);
    connect_prop(scope, direction, [this, id](TextDirection value) {
        static_cast<void>(directional_popup_placement(PopconfirmPlacement::Top, value));
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->direction = value;
            state->needs_measure = true;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Geometry);
            const auto target = state->text_direction;
            target.set(value);
        }
    });
    connect_prop(scope, props.placement_, [this, id](PopconfirmPlacement value) {
        validate_placement(value);
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->placement = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::popconfirm_colors, &theme_runtime::ThemeScope::popconfirm_metrics,
        &theme_runtime::ThemeScope::popconfirm_typography, &theme_runtime::ThemeScope::popconfirm_effects,
        &theme_runtime::ThemeScope::popconfirm_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    s.motion_subscription = theme->capture([this](theme_runtime::DirtyPhase) { synchronize_auxiliary_motion(); },
                                           [theme] { static_cast<void>(theme->motion_enabled()); });
    update_theme(id, true);
    refresh(id);
    if (auto* state = services_->components().state<PopconfirmState>(id)) {
        state->ready = true;
    }
}

void PopconfirmComponentHost::on_window_active(bool active) {
    const auto life = std::weak_ptr{lifetime_};
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            suppress(id);
            if (life.expired()) {
                return;
            }
        }
    }
}

void PopconfirmComponentHost::on_input_boundary_changed() {
    const auto life = std::weak_ptr{lifetime_};
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id);
        }
        if (life.expired()) {
            return;
        }
    }
}

bool PopconfirmComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (!window_active_) {
        return false;
    }
    const auto focus_state = services_->focus().state();
    const auto focus = focus_state.focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    const auto focused_component = record ? std::optional{record->component} : std::optional<runtime::ComponentId>{};
    const auto ids = mounted_;
    std::optional<runtime::ComponentId> escape_target;
    for (auto id : ids) {
        const auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || s->disabled || (s->reference && (s->reference->service_only || !s->reference->service.expired())) ||
            !services_->interactions().component_active(id)) {
            continue;
        }
        if (s->triggers.click &&
            (focus == s->trigger_hit ||
             (focused_component && descendant(*services_, *focused_component, s->snapshot.trigger))) &&
            (event.key == input::Key::enter || event.key == input::Key::space) &&
            event.modifiers == input::KeyModifier::none) {
            const bool wrapper = focus == s->trigger_hit;
            const auto handlers = record ? record->focus_handlers : nullptr;
            const bool native_activation = handlers && handlers->activate && !handlers->text_edit;
            const bool activate = wrapper ? event.action == input::KeyAction::down
                                  : event.key == input::Key::enter
                                      ? event.action == input::KeyAction::down
                                      : event.action == input::KeyAction::up && focus_state.keyboard_pressed;
            if ((!wrapper || !s->native_trigger) && (wrapper || native_activation) && activate && !event.repeat) {
                const bool desired = !s->snapshot.geometry.visible;
                const auto life = std::weak_ptr{lifetime_};
                if (!trigger_can_activate(*services_, id, focus)) {
                    if (life.expired()) {
                        return wrapper;
                    }
                    continue;
                }
                if (life.expired()) {
                    return wrapper;
                }
                if (auto* state = services_->components().state<PopconfirmState>(id);
                    state && !state->disabled && services_->interactions().component_active(id)) {
                    state->latched = desired;
                    state->context_anchor.reset();
                    request_open(id, desired);
                }
            }
            // Focus transfers during layer positioning, after the native trigger
            // has completed Enter or Space activation through FocusManager.
            return wrapper;
        }
        if (event.key == input::Key::escape && event.action == input::KeyAction::down && s->snapshot.geometry.visible) {
            if (!escape_target || !descendant(*services_, *escape_target, id)) {
                escape_target = id;
            }
        }
    }
    if (escape_target) {
        request_open(*escape_target, false);
        return true;
    }
    return false;
}

void PopconfirmComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (!window_active_) {
        return;
    }
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    for (auto id : ids) {
        auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || s->disabled || (s->reference && (s->reference->service_only || !s->reference->service.expired())) ||
            !services_->interactions().component_active(id)) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_trigger =
            hit == s->trigger_hit || (record && descendant(*services_, record->component, s->snapshot.trigger));
        const bool in_popup =
            s->snapshot.geometry.visible && ((record && descendant(*services_, record->component, s->snapshot.popup)) ||
                                             inside(s->snapshot.geometry.bounds, event.x, event.y));
        if (event.action == input::PointerAction::down && event.button == input::PointerButton::secondary) {
            s->context_press = in_trigger && hit ? std::optional{std::pair{event.pointer, *hit}} : std::nullopt;
        }
        const bool context_release =
            s->context_press && hit && s->context_press->first == event.pointer && s->context_press->second == *hit;
        if (event.action == input::PointerAction::cancel ||
            (event.action == input::PointerAction::up && event.button == input::PointerButton::secondary)) {
            s->context_press.reset();
        }
        if (event.action == input::PointerAction::down && event.button == input::PointerButton::primary) {
            s->press_open = in_trigger ? std::optional{s->snapshot.geometry.visible} : std::nullopt;
        }
        s->hovered = event.action != input::PointerAction::cancel && (in_trigger || in_popup);
        if (event.action == input::PointerAction::down && s->snapshot.geometry.visible && !in_trigger && !in_popup) {
            request_open(id, false, false);
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::secondary &&
                   in_trigger && context_release && s->triggers.context_menu &&
                   trigger_can_activate(*services_, id, hit)) {
            if (life.expired()) {
                return;
            }
            s = services_->components().state<PopconfirmState>(id);
            if (s && !s->disabled && services_->interactions().component_active(id)) {
                s->context_anchor = runtime::Point{event.x, event.y};
                s->latched = true;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry);
                request_open(id, true);
            }
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary && hit &&
                   origin == hit && in_trigger && s->triggers.click && trigger_can_activate(*services_, id, hit)) {
            if (life.expired()) {
                return;
            }
            s = services_->components().state<PopconfirmState>(id);
            if (s && !s->disabled && services_->interactions().component_active(id)) {
                const bool open = !s->press_open.value_or(s->snapshot.geometry.visible);
                s->latched = open;
                s->context_anchor.reset();
                request_open(id, open);
            }
        }
        if (life.expired()) {
            return;
        }
        observe(id, services_->animation_time());
        if (life.expired()) {
            return;
        }
        s = services_->components().state<PopconfirmState>(id);
        if (s && (event.action == input::PointerAction::up || event.action == input::PointerAction::cancel)) {
            s->press_open.reset();
        }
    }
}

void PopconfirmComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    if (mounted_.empty()) {
        return;
    }
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    for (auto id : ids) {
        if (!services_->components().contains(id)) {
            continue;
        }
        observe(id, services_->animation_time());
        if (life.expired()) {
            return;
        }
        if (!services_->interactions().component_active(id)) {
            suppress(id);
            if (life.expired()) {
                return;
            }
        }
        auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || !s->motion.painted()) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
        auto& engine = services_->layout();
        const auto popup = services_->components().root(s->snapshot.popup);
        const bool measure = s->needs_measure || s->viewport != viewport ||
                             services_->nodes().require(popup).measure_generation != engine.generation();
        if (measure) {
            layout::FlexLayout message;
            message.main_gap = token.message_gap;
            message.item_policy = layout::FlexItemPolicy::flex;
            message.right_to_left = s->direction == TextDirection::RightToLeft;
            engine.set_layout(services_->components().root(s->message), message);
            layout::FlexLayout body;
            body.direction = layout::FlexDirection::vertical;
            body.main_gap = token.description_gap;
            body.item_policy = layout::FlexItemPolicy::sequential;
            engine.set_layout(services_->components().root(s->body), body);
            layout::FlexLayout footer;
            footer.main_gap = token.button_gap;
            footer.item_policy = layout::FlexItemPolicy::sequential;
            footer.right_to_left = s->direction == TextDirection::RightToLeft;
            engine.set_layout(services_->components().root(s->footer), footer);
            s->popup_size =
                engine.measure_child(popup, {0, std::min(token.max_width, viewport.width), 0, viewport.height});
            s->needs_measure = false;
            s->viewport = viewport;
        }
        if (s->service_anchor && !popconfirm_anchor_live(*s->service_anchor)) {
            suppress(id);
            if (life.expired()) {
                return;
            }
            continue;
        }
        const auto anchor_node = s->service_anchor ? services_->components().root(s->service_anchor->trigger) : s->node;
        const auto& node = services_->nodes().require(anchor_node);
        runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
                             node.bounds.height};
        if (s->context_anchor) {
            anchor = {s->context_anchor->x, s->context_anchor->y, 0, 0};
        }
        const bool visible = s->snapshot.geometry.visible;
        s->snapshot.geometry =
            position_tooltip(anchor, s->popup_size, clip, directional_popup_placement(s->placement, s->direction),
                             token.gap + (s->arrow ? token.arrow_size : 0), s->adjust, s->arrow && s->point_at_center,
                             std::max(12.0F, token.border_radius + 2));
        s->snapshot.geometry.visible = visible;
        if (measure || services_->nodes().require(popup).bounds != s->snapshot.geometry.bounds) {
            engine.place_child(popup, s->snapshot.geometry.bounds);
        }
        const auto bounds = s->snapshot.geometry.bounds;
        const int side = static_cast<int>(s->snapshot.geometry.placement) / 3;
        const float extent = side < 2 ? bounds.width : bounds.height;
        const float radius = std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2);
        const float size = std::min(token.arrow_size, std::max(0.0F, extent / 2 - radius));
        const bool arrow = s->motion.painted() && s->arrow && size > 0;
        s->arrow_path.reset();
        if (services_->components().set_branch_active(s->arrow_component, arrow)) {
            services_->mark_scene_structure_dirty();
        }
        if (arrow) {
            const float center = std::clamp(s->snapshot.geometry.arrow_center, radius + size, extent - radius - size);
            s->snapshot.geometry.arrow_center = center;
            s->arrow_path = graphics::TooltipArrowPath{static_cast<graphics::TooltipArrowSide>(side), center, size,
                                                       token.arrow_size};
            const auto arrow_node = services_->components().root(s->arrow_component);
            runtime::Rect rect{bounds.x + center - size, bounds.y + bounds.height, size * 2, token.arrow_size};
            if (side == 1) {
                rect.y = bounds.y - token.arrow_size;
            } else if (side == 2) {
                rect.x = bounds.x + bounds.width;
                rect.y = bounds.y + center - size;
                rect.width = token.arrow_size;
                rect.height = size * 2;
            } else if (side == 3) {
                rect.x = bounds.x - token.arrow_size;
                rect.y = bounds.y + center - size;
                rect.width = token.arrow_size;
                rect.height = size * 2;
            }
            (void)engine.measure_child(arrow_node, layout::Constraints::fixed(rect.width, rect.height));
            engine.place_child(arrow_node, rect);
        }
        if (s->focus_pending && visible) {
            s->focus_pending = false;
            auto target = first_focus(*services_, s->snapshot.confirm);
            if (!target) {
                target = first_focus(*services_, s->snapshot.popup);
            }
            if (target) {
                services_->focus().defer_focus(target, input::FocusModality::keyboard);
            }
        }
    }
}

void PopconfirmComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect clip{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || !s->motion.painted()) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
        const auto bounds = s->snapshot.geometry.bounds;
        graphics::QuadInstance body;
        body.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
        body.color = foreground(token.background);
        body.opacity = 0;
        body.corner_radius = std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2);
        static_cast<void>(services_->surfaces().update_surface(s->surface, std::span{&body, 1}));
        const graphics::LogicalRoundedRect shape{bounds, body.corner_radius};
        const auto fill =
            s->arrow_path
                ? graphics::make_tooltip_fill_effect(shape, *s->arrow_path, token.background, 1, {},
                                                     graphics::EffectClip{1, clip})
                : graphics::make_shadow_effect(shape, ShadowLayer{ShadowKind::outer, {}, 0, 0, token.background}, {},
                                               graphics::EffectClip{1, clip});
        (void)services_->surfaces().update_content_effects(s->fill, std::span{&fill, 1});
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, body.corner_radius};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
PopconfirmRef::PopconfirmRef() : state_(std::make_shared<detail::PopconfirmRefState>()) {}

PopconfirmRef::PopconfirmRef(const PopconfirmRef& other)
    : state_(other.state_ && other.state_->assigned ? std::make_shared<detail::PopconfirmRefState>(*other.state_)
                                                    : other.state_) {}

PopconfirmRef& PopconfirmRef::operator=(const PopconfirmRef& other) {
    state_ = other.state_ && other.state_->assigned ? std::make_shared<detail::PopconfirmRefState>(*other.state_)
                                                    : other.state_;
    return *this;
}

bool PopconfirmRef::bound() const {
    const auto binding = state_ ? state_->binding.lock() : nullptr;
    return binding && detail::popconfirm_anchor_live(*binding);
}

void Popconfirm(PopconfirmProps props, PopconfirmTrigger trigger, PopconfirmSlots slots) {
    if (!detail::active_popconfirms) {
        throw std::logic_error("Popconfirm requires window component services");
    }
    detail::active_popconfirms->mount(props, trigger, slots);
}
} // namespace ryn
