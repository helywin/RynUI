#include "input/pointer_router.hpp"

#include "input/focus_manager.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::input {

PointerDispatchContext::PointerDispatchContext(
    PointerRouter& router,
    const PointerInputEvent& event,
    PointerEventKind kind,
    InteractionId dispatch_target,
    std::optional<InteractionId> actual_hit_target,
    std::optional<InteractionId> press_origin) noexcept
    : router_(&router),
      event_(&event),
      kind_(kind),
      current_target_(dispatch_target),
      dispatch_target_(dispatch_target),
      actual_hit_target_(actual_hit_target),
      press_origin_(press_origin) {}

const PointerInputEvent& PointerDispatchContext::event() const noexcept {
    return *event_;
}

PointerEventKind PointerDispatchContext::kind() const noexcept {
    return kind_;
}

PointerPropagationPhase PointerDispatchContext::phase() const noexcept {
    return phase_;
}

InteractionId PointerDispatchContext::current_target() const noexcept {
    return current_target_;
}

InteractionId PointerDispatchContext::dispatch_target() const noexcept {
    return dispatch_target_;
}

std::optional<InteractionId> PointerDispatchContext::actual_hit_target() const noexcept {
    return actual_hit_target_;
}

std::optional<InteractionId> PointerDispatchContext::press_origin() const noexcept {
    return press_origin_;
}

bool PointerDispatchContext::propagation_stopped() const noexcept {
    return propagation_stopped_;
}

void PointerDispatchContext::stop_propagation() noexcept {
    propagation_stopped_ = true;
}

bool PointerDispatchContext::capture_pointer() {
    return router_->request_capture(*this);
}

bool PointerDispatchContext::release_pointer_capture() {
    return router_->release_capture(*this);
}

PointerRouter::PointerRouter(
    InteractionRegistry& registry,
    HitTestSnapshot& hit_test,
    runtime::FrameRequestState* frames,
    FocusManager* focus) noexcept
    : registry_(&registry), hit_test_(&hit_test), frames_(frames), focus_(focus) {}

void PointerRouter::reserve(
    std::size_t pointer_capacity,
    std::size_t route_capacity) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    if (dispatching_) {
        throw std::logic_error("PointerRouter capacity cannot change during dispatch");
    }
    states_.reserve(pointer_capacity);
    route_scratch_.reserve(route_capacity);
    next_hover_scratch_.reserve(route_capacity);
    previous_hover_scratch_.reserve(route_capacity);
    route_capacity_ = std::max(route_capacity_, route_capacity);
    for (auto& pointer : states_) {
        pointer.hover_path.reserve(route_capacity_);
    }
}

void PointerRouter::dispatch(const PointerInputEvent& event) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    if (dispatching_) {
        ++diagnostics_.reentrant_rejections;
        throw std::logic_error("PointerRouter dispatch cannot reenter");
    }
    if (!is_valid(event)) {
        throw std::invalid_argument("PointerRouter rejected an invalid pointer event");
    }

    dispatching_ = true;
    frame_requested_during_dispatch_ = false;
    PointerState* pointer = nullptr;
    try {
        pointer = &state_for(event.pointer);
        ++diagnostics_.input_events;
        pointer->position = {event.x, event.y};
        sanitize_before_dispatch(*pointer, event);

        const auto actual_target = event.action == PointerAction::cancel
            ? std::optional<InteractionId>{}
            : hit_test_->hit_test(pointer->position);
        if (event.action != PointerAction::cancel) {
            update_hover(*pointer, actual_target, event);
        }
        const auto kind = event_kind(event.action);

        if (event.action == PointerAction::down
                && event.button == PointerButton::primary) {
            if (focus_ != nullptr) {
                static_cast<void>(focus_->focus_from_pointer(actual_target));
            }
            if (pointer->primary_down || pointer->capture.has_value()
                    || pointer->press_origin.has_value()) {
                clear_primary_state(*pointer, true);
            }
            pointer->primary_down = true;
            pointer->press_origin = actual_target;
            if (actual_target.has_value()) {
                request_frame();
            }
        }

        const auto route_target = pointer->capture.has_value()
            ? pointer->capture
            : actual_target;
        if (route_target.has_value()) {
            dispatch_route(
                *pointer,
                event,
                kind,
                *route_target,
                actual_target);
        }

        if (event.action == PointerAction::up
                && event.button == PointerButton::primary) {
            clear_primary_state(*pointer, false);
            if (event.pointer.device == PointerDevice::touch) {
                clear_hover(*pointer, event);
            }
        } else if (event.action == PointerAction::cancel) {
            clear_primary_state(*pointer, true);
            clear_hover(*pointer, event);
        }
        prune_invalid_state(*pointer);
        dispatching_ = false;
    } catch (...) {
        if (pointer != nullptr) {
            abort_pointer(*pointer);
        }
        dispatching_ = false;
        throw;
    }
}

void PointerRouter::cancel_all() {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    const bool invoke_handlers = !dispatching_;
    if (invoke_handlers) {
        dispatching_ = true;
        frame_requested_during_dispatch_ = false;
    }
    try {
        for (auto& pointer : states_) {
            const PointerInputEvent cancel{
                pointer.pointer,
                PointerAction::cancel,
                PointerButton::none,
                pointer.position.x,
                pointer.position.y,
            };
            const auto target = pointer.capture.has_value()
                ? pointer.capture
                : pointer.press_origin;
            if (invoke_handlers && target.has_value()
                    && registry_->find(*target) != nullptr) {
                dispatch_route(
                    pointer,
                    cancel,
                    PointerEventKind::cancel,
                    *target,
                    std::nullopt);
            }
            clear_primary_state(pointer, true);
            if (invoke_handlers) {
                clear_hover(pointer, cancel);
            } else if (!pointer.hover_path.empty()) {
                diagnostics_.hover_leaves += pointer.hover_path.size();
                pointer.hover_path.clear();
                request_frame();
            }
        }
        if (invoke_handlers) {
            dispatching_ = false;
        }
    } catch (...) {
        for (auto& pointer : states_) {
            abort_pointer(pointer);
        }
        if (invoke_handlers) {
            dispatching_ = false;
        }
        throw;
    }
}

void PointerRouter::cancel_interaction(InteractionId interaction) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    if (focus_ != nullptr) {
        focus_->cancel_interaction(interaction);
    }
    const bool invoke_handlers = !dispatching_;
    if (invoke_handlers) {
        dispatching_ = true;
        frame_requested_during_dispatch_ = false;
    }
    try {
        for (auto& pointer : states_) {
            const bool owns_primary = pointer.capture == interaction
                || pointer.press_origin == interaction;
            const bool in_hover = std::find(
                pointer.hover_path.begin(),
                pointer.hover_path.end(),
                interaction) != pointer.hover_path.end();
            if (!owns_primary && !in_hover) {
                continue;
            }
            const PointerInputEvent cancel{
                pointer.pointer,
                PointerAction::cancel,
                PointerButton::none,
                pointer.position.x,
                pointer.position.y,
            };
            if (invoke_handlers && owns_primary
                    && registry_->find(interaction) != nullptr) {
                dispatch_route(
                    pointer,
                    cancel,
                    PointerEventKind::cancel,
                    interaction,
                    std::nullopt);
            }
            if (owns_primary) {
                clear_primary_state(pointer, true);
            }
            if (in_hover && invoke_handlers) {
                clear_hover(pointer, cancel);
            } else if (in_hover) {
                diagnostics_.hover_leaves += pointer.hover_path.size();
                pointer.hover_path.clear();
                request_frame();
            }
        }
        if (invoke_handlers) {
            dispatching_ = false;
        }
    } catch (...) {
        for (auto& pointer : states_) {
            abort_pointer(pointer);
        }
        if (invoke_handlers) {
            dispatching_ = false;
        }
        throw;
    }
}

std::optional<PointerStateSnapshot> PointerRouter::state(
    PointerIdentity pointer) const {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    const auto* current = find_state(pointer);
    if (current == nullptr) {
        return std::nullopt;
    }
    return PointerStateSnapshot{
        current->pointer,
        current->position,
        current->primary_down,
        current->hover_path.empty()
            ? std::nullopt
            : std::optional<InteractionId>{current->hover_path.back()},
        current->capture,
        current->press_origin,
    };
}

std::size_t PointerRouter::pointer_count() const {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("PointerRouter can only be used on its owner thread");
    }
    return states_.size();
}

const PointerRouterDiagnostics& PointerRouter::diagnostics() const noexcept {
    return diagnostics_;
}

PointerRouter::PointerState* PointerRouter::find_state(
    PointerIdentity pointer) noexcept {
    const auto found = std::find_if(
        states_.begin(),
        states_.end(),
        [&](const auto& current) { return current.pointer == pointer; });
    return found == states_.end() ? nullptr : &*found;
}

const PointerRouter::PointerState* PointerRouter::find_state(
    PointerIdentity pointer) const noexcept {
    const auto found = std::find_if(
        states_.begin(),
        states_.end(),
        [&](const auto& current) { return current.pointer == pointer; });
    return found == states_.end() ? nullptr : &*found;
}

PointerRouter::PointerState& PointerRouter::state_for(PointerIdentity pointer) {
    if (auto* current = find_state(pointer)) {
        return *current;
    }
    PointerState state;
    state.pointer = pointer;
    state.hover_path.reserve(route_capacity_);
    states_.push_back(std::move(state));
    return states_.back();
}

bool PointerRouter::build_route(
    InteractionId target,
    std::vector<InteractionId>& route) {
    route.clear();
    auto current = std::optional<InteractionId>{target};
    while (current.has_value()) {
        const auto* record = registry_->find(*current);
        if (record == nullptr) {
            ++diagnostics_.stale_skips;
            route.clear();
            return false;
        }
        route.push_back(*current);
        current = record->parent;
    }
    std::reverse(route.begin(), route.end());
    return !route.empty();
}

void PointerRouter::update_hover(
    PointerState& state,
    std::optional<InteractionId> actual_target,
    const PointerInputEvent& event) {
    next_hover_scratch_.clear();
    if (actual_target.has_value()) {
        static_cast<void>(build_route(*actual_target, next_hover_scratch_));
    }
    if (state.hover_path == next_hover_scratch_) {
        return;
    }

    previous_hover_scratch_.assign(
        state.hover_path.begin(),
        state.hover_path.end());
    std::size_t common = 0;
    while (common < previous_hover_scratch_.size()
            && common < next_hover_scratch_.size()
            && previous_hover_scratch_[common] == next_hover_scratch_[common]) {
        ++common;
    }
    state.hover_path.assign(
        next_hover_scratch_.begin(),
        next_hover_scratch_.end());
    request_frame();

    for (std::size_t index = previous_hover_scratch_.size(); index > common; --index) {
        ++diagnostics_.hover_leaves;
        invoke_direct(
            state,
            event,
            PointerEventKind::leave,
            previous_hover_scratch_[index - 1],
            actual_target);
    }
    for (std::size_t index = common; index < next_hover_scratch_.size(); ++index) {
        ++diagnostics_.hover_enters;
        invoke_direct(
            state,
            event,
            PointerEventKind::enter,
            next_hover_scratch_[index],
            actual_target);
    }
}

void PointerRouter::clear_hover(
    PointerState& state,
    const PointerInputEvent& event) {
    if (state.hover_path.empty()) {
        return;
    }
    previous_hover_scratch_.assign(
        state.hover_path.begin(),
        state.hover_path.end());
    state.hover_path.clear();
    request_frame();
    for (auto iterator = previous_hover_scratch_.rbegin();
         iterator != previous_hover_scratch_.rend();
         ++iterator) {
        ++diagnostics_.hover_leaves;
        invoke_direct(
            state,
            event,
            PointerEventKind::leave,
            *iterator,
            std::nullopt);
    }
}

void PointerRouter::dispatch_route(
    PointerState& state,
    const PointerInputEvent& event,
    PointerEventKind kind,
    InteractionId target,
    std::optional<InteractionId> actual_target) {
    if (!build_route(target, route_scratch_)) {
        return;
    }
    ++diagnostics_.routes_dispatched;
    diagnostics_.route_entries += route_scratch_.size();
    PointerDispatchContext context(
        *this,
        event,
        kind,
        target,
        actual_target,
        state.press_origin);

    if (route_scratch_.size() > 1) {
        for (std::size_t index = 0; index + 1 < route_scratch_.size(); ++index) {
            if (invoke_handler(
                    state,
                    context,
                    route_scratch_[index],
                    PointerPropagationPhase::capture)) {
                return;
            }
        }
    }
    if (invoke_handler(
            state,
            context,
            route_scratch_.back(),
            PointerPropagationPhase::target)) {
        return;
    }
    if (route_scratch_.size() > 1) {
        for (std::size_t index = route_scratch_.size() - 1; index > 0; --index) {
            if (invoke_handler(
                    state,
                    context,
                    route_scratch_[index - 1],
                    PointerPropagationPhase::bubble)) {
                return;
            }
        }
    }
}

void PointerRouter::invoke_direct(
    PointerState& state,
    const PointerInputEvent& event,
    PointerEventKind kind,
    InteractionId target,
    std::optional<InteractionId> actual_target) {
    PointerDispatchContext context(
        *this,
        event,
        kind,
        target,
        actual_target,
        state.press_origin);
    static_cast<void>(invoke_handler(
        state,
        context,
        target,
        PointerPropagationPhase::target));
}

bool PointerRouter::invoke_handler(
    PointerState& state,
    PointerDispatchContext& context,
    InteractionId current,
    PointerPropagationPhase phase) {
    const auto* record = registry_->find(current);
    if (record == nullptr) {
        ++diagnostics_.stale_skips;
        prune_invalid_state(state);
        return context.propagation_stopped_;
    }
    const bool cleanup_event = context.kind_ == PointerEventKind::cancel
        || context.kind_ == PointerEventKind::leave;
    if (!record->eligible && !cleanup_event) {
        ++diagnostics_.stale_skips;
        prune_invalid_state(state);
        return context.propagation_stopped_;
    }

    const auto handlers = record->handlers;
    if (handlers == nullptr) {
        return context.propagation_stopped_;
    }
    const PointerEventHandler* handler = nullptr;
    switch (phase) {
    case PointerPropagationPhase::capture:
        handler = &handlers->capture;
        break;
    case PointerPropagationPhase::target:
        handler = &handlers->target;
        break;
    case PointerPropagationPhase::bubble:
        handler = &handlers->bubble;
        break;
    }
    if (handler == nullptr || !*handler) {
        return context.propagation_stopped_;
    }

    context.current_target_ = current;
    context.phase_ = phase;
    (*handler)(context);
    ++diagnostics_.handlers_invoked;
    prune_invalid_state(state);
    return context.propagation_stopped_;
}

void PointerRouter::sanitize_before_dispatch(
    PointerState& state,
    const PointerInputEvent& event) {
    if (state.capture.has_value()) {
        const auto* capture = registry_->find(*state.capture);
        if (capture == nullptr || !capture->eligible) {
            const auto stale_capture = state.capture;
            if (capture != nullptr) {
                dispatch_route(
                    state,
                    PointerInputEvent{
                        state.pointer,
                        PointerAction::cancel,
                        PointerButton::none,
                        event.x,
                        event.y,
                    },
                    PointerEventKind::cancel,
                    *stale_capture,
                    std::nullopt);
            }
            clear_primary_state(state, true);
        }
    }
    if (state.press_origin.has_value()) {
        const auto* press = registry_->find(*state.press_origin);
        if (press == nullptr || !press->eligible) {
            state.press_origin.reset();
        }
    }
}

void PointerRouter::prune_invalid_state(PointerState& state) {
    if (state.capture.has_value()) {
        const auto* capture = registry_->find(*state.capture);
        if (capture == nullptr || !capture->eligible) {
            clear_primary_state(state, true);
        }
    }
    if (state.press_origin.has_value()) {
        const auto* press = registry_->find(*state.press_origin);
        if (press == nullptr || !press->eligible) {
            state.press_origin.reset();
        }
    }
}

void PointerRouter::clear_primary_state(
    PointerState& state,
    bool count_cancel) {
    const bool had_primary = state.primary_down
        || state.capture.has_value()
        || state.press_origin.has_value();
    if (state.capture.has_value()) {
        state.capture.reset();
        ++diagnostics_.captures_released;
    }
    state.press_origin.reset();
    state.primary_down = false;
    if (count_cancel && had_primary) {
        ++diagnostics_.cancels;
    }
    if (had_primary) {
        request_frame();
    }
}

void PointerRouter::abort_pointer(PointerState& state) noexcept {
    clear_primary_state(state, true);
}

void PointerRouter::request_frame() noexcept {
    if (dispatching_ && frame_requested_during_dispatch_) {
        return;
    }
    frame_requested_during_dispatch_ = dispatching_;
    ++diagnostics_.frame_requests;
    if (frames_ != nullptr) {
        frames_->request_frame();
    }
}

bool PointerRouter::request_capture(
    const PointerDispatchContext& context) {
    if (!dispatching_
            || context.phase_ != PointerPropagationPhase::target
            || context.kind_ != PointerEventKind::down
            || context.current_target_ != context.dispatch_target_
            || context.event_->action != PointerAction::down
            || context.event_->button != PointerButton::primary) {
        return false;
    }
    auto* state = find_state(context.event_->pointer);
    if (state == nullptr || !state->primary_down
            || registry_->find(context.current_target_) == nullptr) {
        return false;
    }
    if (state->capture == context.current_target_) {
        return false;
    }
    if (state->capture.has_value()) {
        ++diagnostics_.captures_released;
    }
    state->capture = context.current_target_;
    ++diagnostics_.captures_started;
    request_frame();
    return true;
}

bool PointerRouter::release_capture(
    const PointerDispatchContext& context) {
    auto* state = find_state(context.event_->pointer);
    if (state == nullptr || state->capture != context.current_target_) {
        return false;
    }
    state->capture.reset();
    ++diagnostics_.captures_released;
    request_frame();
    return true;
}

PointerEventKind PointerRouter::event_kind(PointerAction action) {
    switch (action) {
    case PointerAction::move:
        return PointerEventKind::move;
    case PointerAction::down:
        return PointerEventKind::down;
    case PointerAction::up:
        return PointerEventKind::up;
    case PointerAction::cancel:
        return PointerEventKind::cancel;
    case PointerAction::invalid:
        break;
    }
    throw std::invalid_argument("Pointer action cannot be dispatched");
}

} // namespace ryn::input
