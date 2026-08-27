#include "input/focus_manager.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::input {

FocusManager::FocusManager(
    InteractionRegistry& registry,
    runtime::FrameRequestState* frames) noexcept
    : registry_(&registry), frames_(frames) {}

void FocusManager::reserve(std::size_t focus_capacity) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("FocusManager can only be used on its owner thread");
    }
    if (dispatching_) {
        throw std::logic_error("FocusManager capacity cannot change during dispatch");
    }
    focus_order_.reserve(focus_capacity);
}

void FocusManager::dispatch(const KeyboardInputEvent& event) {
    begin_operation();
    try {
        if (!is_valid(event)) {
            throw std::invalid_argument("FocusManager rejected an invalid keyboard event");
        }
        ++diagnostics_.keyboard_events;
        sanitize_internal();

        if (!window_active_) {
            end_operation();
            return;
        }

        if (event.key == Key::tab) {
            if (event.action == KeyAction::down) {
                static_cast<void>(cancel_keyboard_press_internal(true));
                rebuild_focus_order();
                ++diagnostics_.traversals;
                if (focus_order_.empty()) {
                    static_cast<void>(clear_focus_internal(false));
                    modality_ = FocusModality::keyboard;
                } else {
                    auto found = focused_.has_value()
                        ? std::find(focus_order_.begin(), focus_order_.end(), *focused_)
                        : focus_order_.end();
                    const bool reverse = has_modifier(
                        event.modifiers, KeyModifier::shift);
                    InteractionId next;
                    if (found == focus_order_.end()) {
                        next = reverse ? focus_order_.back() : focus_order_.front();
                    } else if (reverse) {
                        next = found == focus_order_.begin()
                            ? focus_order_.back()
                            : *std::prev(found);
                    } else {
                        next = std::next(found) == focus_order_.end()
                            ? focus_order_.front()
                            : *std::next(found);
                    }
                    static_cast<void>(set_focus_internal(
                        next, FocusModality::keyboard));
                }
            }
            end_operation();
            return;
        }

        if (!focused_.has_value()) {
            end_operation();
            return;
        }

        if (modality_ != FocusModality::keyboard) {
            const auto target = focused_;
            modality_ = FocusModality::keyboard;
            request_frame();
            notify_state(*target, presentation_for(*target));
            sanitize_internal();
        }
        if (!focused_.has_value()) {
            end_operation();
            return;
        }

        if (event.key == Key::enter) {
            if (event.action == KeyAction::down && !event.repeat
                    && !keyboard_press_.has_value()) {
                activate(*focused_);
            }
            end_operation();
            return;
        }

        if (event.key == Key::space) {
            if (event.action == KeyAction::down) {
                if (!event.repeat && !keyboard_press_.has_value()
                        && activation_permitted(*focused_)) {
                    keyboard_press_ = focused_;
                    ++diagnostics_.keyboard_presses;
                    request_frame();
                    notify_state(*focused_, presentation_for(*focused_));
                    sanitize_internal();
                }
            } else if (keyboard_press_.has_value()) {
                const auto pressed = *keyboard_press_;
                static_cast<void>(cancel_keyboard_press_internal(true));
                if (focused_ == pressed && window_active_ && can_focus(pressed)) {
                    activate(pressed);
                }
            }
        }
        end_operation();
    } catch (...) {
        if (keyboard_press_.has_value()) {
            keyboard_press_.reset();
            ++diagnostics_.keyboard_press_cancels;
            request_frame();
        }
        end_operation();
        throw;
    }
}

bool FocusManager::focus_from_pointer(
    std::optional<InteractionId> target) {
    begin_operation();
    try {
        ++diagnostics_.pointer_focus_requests;
        sanitize_internal();
        static_cast<void>(cancel_keyboard_press_internal(true));
        const auto next = focus_candidate(target);
        const bool changed = window_active_
            ? set_focus_internal(next, FocusModality::pointer)
            : false;
        sanitize_internal();
        end_operation();
        return changed;
    } catch (...) {
        end_operation();
        throw;
    }
}

bool FocusManager::request_focus(
    InteractionId target,
    FocusModality modality) {
    begin_operation();
    try {
        sanitize_internal();
        const bool changed = window_active_ && can_focus(target)
            ? set_focus_internal(target, modality)
            : false;
        sanitize_internal();
        end_operation();
        return changed;
    } catch (...) {
        end_operation();
        throw;
    }
}

bool FocusManager::clear_focus() {
    begin_operation();
    try {
        const bool changed = clear_focus_internal(false);
        end_operation();
        return changed;
    } catch (...) {
        end_operation();
        throw;
    }
}

void FocusManager::set_window_active(bool active) {
    begin_operation();
    try {
        sanitize_internal();
        if (window_active_ == active) {
            end_operation();
            return;
        }
        window_active_ = active;
        if (!active) {
            const bool had_press = keyboard_press_.has_value();
            keyboard_press_.reset();
            if (had_press) {
                ++diagnostics_.keyboard_press_cancels;
            }
        }
        if (focused_.has_value()) {
            request_frame();
            notify_state(*focused_, presentation_for(*focused_));
            sanitize_internal();
        }
        end_operation();
    } catch (...) {
        end_operation();
        throw;
    }
}

void FocusManager::synchronize() {
    begin_operation();
    try {
        sanitize_internal();
        end_operation();
    } catch (...) {
        if (keyboard_press_.has_value()) {
            keyboard_press_.reset();
            ++diagnostics_.keyboard_press_cancels;
            request_frame();
        }
        end_operation();
        throw;
    }
}

void FocusManager::cancel_interaction(InteractionId interaction) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("FocusManager can only be used on its owner thread");
    }
    const bool affected = focused_ == interaction
        || keyboard_press_ == interaction;
    if (!affected) {
        return;
    }
    if (dispatching_) {
        const bool had_focus = focused_ == interaction;
        const bool had_press = keyboard_press_ == interaction;
        if (had_press) {
            keyboard_press_.reset();
        }
        if (had_focus) {
            focused_.reset();
        }
        if (had_press) {
            ++diagnostics_.keyboard_press_cancels;
        }
        if (had_focus) {
            ++diagnostics_.focus_changes;
            ++diagnostics_.focus_clears;
        }
        request_frame();
        return;
    }
    begin_operation();
    try {
        if (focused_ == interaction) {
            static_cast<void>(clear_focus_internal(false));
        } else {
            static_cast<void>(cancel_keyboard_press_internal(false));
        }
        end_operation();
    } catch (...) {
        end_operation();
        throw;
    }
}

FocusSnapshot FocusManager::state() const {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("FocusManager can only be used on its owner thread");
    }
    return {
        focused_,
        modality_,
        window_active_,
        focused_.has_value() && window_active_
            && modality_ == FocusModality::keyboard,
        keyboard_press_.has_value(),
    };
}

const FocusManagerDiagnostics& FocusManager::diagnostics() const noexcept {
    return diagnostics_;
}

void FocusManager::begin_operation() {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("FocusManager can only be used on its owner thread");
    }
    if (dispatching_) {
        ++diagnostics_.reentrant_rejections;
        throw std::logic_error("FocusManager dispatch cannot reenter");
    }
    dispatching_ = true;
    frame_requested_during_dispatch_ = false;
}

void FocusManager::end_operation() noexcept {
    dispatching_ = false;
}

void FocusManager::rebuild_focus_order() {
    focus_order_.clear();
    for (const auto interaction : registry_->declaration_order()) {
        const auto* record = registry_->find(interaction);
        if (record != nullptr && record->eligible && record->focusable) {
            focus_order_.push_back(interaction);
        }
    }
}

bool FocusManager::can_focus(InteractionId target) const {
    const auto* record = registry_->find(target);
    return record != nullptr && record->eligible && record->focusable;
}

std::optional<InteractionId> FocusManager::focus_candidate(
    std::optional<InteractionId> target) const {
    auto current = target;
    while (current.has_value()) {
        const auto* record = registry_->find(*current);
        if (record == nullptr || !record->eligible) {
            return std::nullopt;
        }
        if (record->focusable) {
            return current;
        }
        current = record->parent;
    }
    return std::nullopt;
}

bool FocusManager::set_focus_internal(
    std::optional<InteractionId> target,
    FocusModality modality) {
    if (target.has_value() && !can_focus(*target)) {
        target.reset();
    }
    if (focused_ == target && modality_ == modality) {
        return false;
    }

    if (focused_ == target && !focused_.has_value()) {
        modality_ = modality;
        return true;
    }

    const auto previous = focused_;
    const auto previous_modality = modality_;
    if (keyboard_press_.has_value() && keyboard_press_ != target) {
        static_cast<void>(cancel_keyboard_press_internal(true));
    }
    focused_ = target;
    modality_ = modality;
    if (previous != focused_) {
        ++diagnostics_.focus_changes;
        if (!focused_.has_value()) {
            ++diagnostics_.focus_clears;
        }
    }
    request_frame();

    if (previous.has_value() && previous != focused_) {
        notify_state(*previous, {});
    }
    if (focused_.has_value() && !can_focus(*focused_)) {
        static_cast<void>(clear_focus_internal(true));
        return true;
    }
    if (focused_.has_value()) {
        if (focused_ != previous || previous_modality != modality_) {
            notify_state(*focused_, presentation_for(*focused_));
        }
    }
    if (focused_.has_value() && !can_focus(*focused_)) {
        static_cast<void>(clear_focus_internal(true));
    }
    return true;
}

bool FocusManager::clear_focus_internal(bool stale) {
    if (!focused_.has_value() && !keyboard_press_.has_value()) {
        return false;
    }
    const auto previous = focused_;
    const bool had_press = keyboard_press_.has_value();
    keyboard_press_.reset();
    focused_.reset();
    if (had_press) {
        ++diagnostics_.keyboard_press_cancels;
    }
    if (previous.has_value()) {
        ++diagnostics_.focus_changes;
        ++diagnostics_.focus_clears;
        if (stale) {
            ++diagnostics_.stale_clears;
        }
        request_frame();
        notify_state(*previous, {});
    } else if (had_press) {
        request_frame();
    }
    return true;
}

bool FocusManager::cancel_keyboard_press_internal(bool notify) {
    if (!keyboard_press_.has_value()) {
        return false;
    }
    const auto pressed = *keyboard_press_;
    keyboard_press_.reset();
    ++diagnostics_.keyboard_press_cancels;
    request_frame();
    if (notify && focused_ == pressed) {
        notify_state(pressed, presentation_for(pressed));
    }
    return true;
}

void FocusManager::sanitize_internal() {
    if (focused_.has_value() && !can_focus(*focused_)) {
        static_cast<void>(clear_focus_internal(true));
        return;
    }
    if (keyboard_press_.has_value()) {
        const auto pressed = *keyboard_press_;
        if (focused_ != pressed || !window_active_
                || !activation_permitted(pressed)) {
            static_cast<void>(cancel_keyboard_press_internal(true));
        }
    }
}

bool FocusManager::activation_permitted(InteractionId target) {
    if (!window_active_ || focused_ != target) {
        ++diagnostics_.activation_rejections;
        return false;
    }
    const auto* record = registry_->find(target);
    if (record == nullptr || !record->eligible || !record->focusable
            || record->focus_handlers == nullptr
            || !record->focus_handlers->activate) {
        ++diagnostics_.activation_rejections;
        return false;
    }
    const auto handlers = record->focus_handlers;
    if (handlers->activation_allowed && !handlers->activation_allowed()) {
        ++diagnostics_.activation_rejections;
        return false;
    }
    const auto* current = registry_->find(target);
    if (current == nullptr || !current->eligible || !current->focusable
            || !window_active_ || focused_ != target) {
        ++diagnostics_.activation_rejections;
        return false;
    }
    return true;
}

void FocusManager::activate(InteractionId target) {
    if (!activation_permitted(target)) {
        return;
    }
    const auto* record = registry_->find(target);
    if (record == nullptr || record->focus_handlers == nullptr
            || !record->focus_handlers->activate) {
        ++diagnostics_.activation_rejections;
        return;
    }
    const auto handlers = record->focus_handlers;
    ++diagnostics_.activations;
    handlers->activate();
    sanitize_internal();
}

void FocusManager::notify_state(
    InteractionId target,
    FocusPresentation presentation) {
    const auto* record = registry_->find(target);
    if (record == nullptr || record->focus_handlers == nullptr
            || !record->focus_handlers->state_changed) {
        return;
    }
    const auto handlers = record->focus_handlers;
    ++diagnostics_.state_notifications;
    handlers->state_changed(presentation);
}

void FocusManager::request_frame() noexcept {
    if (dispatching_ && frame_requested_during_dispatch_) {
        return;
    }
    frame_requested_during_dispatch_ = dispatching_;
    ++diagnostics_.frame_requests;
    if (frames_ != nullptr) {
        frames_->request_frame();
    }
}

FocusPresentation FocusManager::presentation_for(
    InteractionId target) const noexcept {
    const bool focused = focused_ == target;
    return {
        focused,
        focused && window_active_ && modality_ == FocusModality::keyboard,
        focused && keyboard_press_ == target,
    };
}

} // namespace ryn::input
