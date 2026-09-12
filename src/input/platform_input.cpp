#include "input/platform_input.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <type_traits>

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
    return key >= Key::tab && key <= Key::y;
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

bool is_valid(const ScrollInputEvent& event) noexcept {
    return std::isfinite(event.delta_x)
        && std::isfinite(event.delta_y)
        && std::isfinite(event.x)
        && std::isfinite(event.y)
        && (event.delta_x != 0.0F || event.delta_y != 0.0F);
}

bool is_valid(const KeyboardInputEvent& event) noexcept {
    const auto modifier_bits = static_cast<std::uint8_t>(event.modifiers);
    return is_key(event.key)
        && is_key_action(event.action)
        && (event.primary_modifier == KeyModifier::control || event.primary_modifier == KeyModifier::meta)
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

PlatformInputBatch::PlatformInputBatch(std::size_t max_events,
    std::size_t max_payload_bytes) noexcept
    : max_events_(max_events), max_payload_bytes_(max_payload_bytes) {}

void PlatformInputBatch::reserve(std::size_t capacity) {
    if (capacity > max_events_) throw std::length_error("Input batch reserve exceeds limit");
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
    const auto bytes = std::visit([](const auto& value) -> std::size_t {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, TextCommitted> || std::is_same_v<T, CompositionChanged>)
            return value.text.size_bytes();
        else if constexpr (std::is_same_v<T, CandidatesChanged>) return payload_bytes(value);
        else return 0;
    }, event);
    if (events_.size() >= max_events_ || bytes > max_payload_bytes_ - payload_bytes_)
        throw std::length_error("Input batch capacity exceeded");
    events_.push_back(std::move(event));
    payload_bytes_ += bytes;
    return true;
}

void PlatformInputBatch::clear() noexcept {
    events_.clear();
    payload_bytes_ = 0;
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

std::size_t PlatformInputBatch::payload_size_bytes() const noexcept {
    return payload_bytes_;
}

std::uint64_t PlatformInputBatch::coalesced_move_count() const noexcept {
    return coalesced_move_count_;
}

std::span<const PlatformInputEvent> PlatformInputBatch::events() const noexcept {
    return events_;
}

} // namespace ryn::input
