#include "input/platform_input.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn::input {
namespace {

constexpr std::uint8_t all_modifier_bits =
    static_cast<std::uint8_t>(KeyModifier::shift)
    | static_cast<std::uint8_t>(KeyModifier::control)
    | static_cast<std::uint8_t>(KeyModifier::alt)
    | static_cast<std::uint8_t>(KeyModifier::meta);

bool is_pointer_action(PointerAction action) noexcept {
    return action == PointerAction::move
        || action == PointerAction::down
        || action == PointerAction::up
        || action == PointerAction::cancel;
}

bool is_key_action(KeyAction action) noexcept {
    return action == KeyAction::down || action == KeyAction::up;
}

bool is_key(Key key) noexcept {
    return key == Key::tab || key == Key::enter || key == Key::space;
}

bool is_window_action(WindowInputAction action) noexcept {
    return action == WindowInputAction::focus_gained
        || action == WindowInputAction::focus_lost
        || action == WindowInputAction::resized;
}

bool is_consecutive_move(
    const PlatformInputEvent& previous,
    const PlatformInputEvent& current) noexcept {
    const auto* previous_pointer = std::get_if<PointerInputEvent>(&previous);
    const auto* current_pointer = std::get_if<PointerInputEvent>(&current);
    return previous_pointer != nullptr
        && current_pointer != nullptr
        && previous_pointer->action == PointerAction::move
        && current_pointer->action == PointerAction::move
        && previous_pointer->pointer == current_pointer->pointer;
}

} // namespace

bool is_valid(const PointerIdentity& identity) noexcept {
    if (identity.device == PointerDevice::mouse) {
        return identity.device_id == 0 && identity.pointer_id == 0;
    }
    return identity.device == PointerDevice::touch;
}

bool is_valid(const PointerInputEvent& event) noexcept {
    if (!is_valid(event.pointer)
            || !is_pointer_action(event.action)
            || !std::isfinite(event.x)
            || !std::isfinite(event.y)) {
        return false;
    }
    if (event.action == PointerAction::down || event.action == PointerAction::up) {
        return event.button == PointerButton::primary
            || event.button == PointerButton::secondary;
    }
    return event.button == PointerButton::none;
}

bool is_valid(const KeyboardInputEvent& event) noexcept {
    const auto modifier_bits = static_cast<std::uint8_t>(event.modifiers);
    return is_key(event.key)
        && is_key_action(event.action)
        && (modifier_bits & static_cast<std::uint8_t>(~all_modifier_bits)) == 0;
}

bool is_valid(const WindowInputEvent& event) noexcept {
    if (!is_window_action(event.action)) {
        return false;
    }
    if (event.action == WindowInputAction::resized) {
        return event.width > 0 && event.height > 0;
    }
    return event.width == 0 && event.height == 0;
}

bool is_valid(const PlatformInputEvent& event) noexcept {
    return std::visit([](const auto& value) { return is_valid(value); }, event);
}

void PlatformInputBatch::reserve(std::size_t capacity) {
    events_.reserve(capacity);
}

bool PlatformInputBatch::append(PlatformInputEvent event) {
    if (!is_valid(event)) {
        throw std::invalid_argument("PlatformInputBatch rejected an invalid event");
    }
    if (!events_.empty() && is_consecutive_move(events_.back(), event)) {
        events_.back() = std::move(event);
        ++coalesced_move_count_;
        return false;
    }
    events_.push_back(std::move(event));
    return true;
}

void PlatformInputBatch::clear() noexcept {
    events_.clear();
    coalesced_move_count_ = 0;
}

bool PlatformInputBatch::empty() const noexcept {
    return events_.empty();
}

std::size_t PlatformInputBatch::size() const noexcept {
    return events_.size();
}

std::size_t PlatformInputBatch::capacity() const noexcept {
    return events_.capacity();
}

std::uint64_t PlatformInputBatch::coalesced_move_count() const noexcept {
    return coalesced_move_count_;
}

std::span<const PlatformInputEvent> PlatformInputBatch::events() const noexcept {
    return events_;
}

} // namespace ryn::input
