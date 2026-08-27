#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace ryn::input {

enum class PointerDevice : std::uint8_t {
    invalid,
    mouse,
    touch,
};

struct PointerIdentity {
    PointerDevice device{PointerDevice::invalid};
    std::uint64_t device_id{0};
    std::uint64_t pointer_id{0};

    [[nodiscard]] static constexpr PointerIdentity mouse() noexcept {
        return {PointerDevice::mouse, 0, 0};
    }

    [[nodiscard]] static constexpr PointerIdentity touch(
        std::uint64_t device,
        std::uint64_t pointer) noexcept {
        return {PointerDevice::touch, device, pointer};
    }

    friend bool operator==(const PointerIdentity&, const PointerIdentity&) = default;
};

enum class PointerAction : std::uint8_t {
    invalid,
    move,
    down,
    up,
    cancel,
};

enum class PointerButton : std::uint8_t {
    none,
    primary,
    secondary,
};

struct PointerInputEvent {
    PointerIdentity pointer;
    PointerAction action{PointerAction::invalid};
    PointerButton button{PointerButton::none};
    float x{0.0F};
    float y{0.0F};

    friend bool operator==(const PointerInputEvent&, const PointerInputEvent&) = default;
};

enum class Key : std::uint8_t {
    invalid,
    tab,
    enter,
    space,
};

enum class KeyAction : std::uint8_t {
    invalid,
    down,
    up,
};

enum class KeyModifier : std::uint8_t {
    none = 0,
    shift = 1U << 0U,
    control = 1U << 1U,
    alt = 1U << 2U,
    meta = 1U << 3U,
};

[[nodiscard]] constexpr KeyModifier operator|(
    KeyModifier lhs,
    KeyModifier rhs) noexcept {
    return static_cast<KeyModifier>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr bool has_modifier(
    KeyModifier modifiers,
    KeyModifier modifier) noexcept {
    return (static_cast<std::uint8_t>(modifiers)
            & static_cast<std::uint8_t>(modifier)) != 0;
}

struct KeyboardInputEvent {
    Key key{Key::invalid};
    KeyAction action{KeyAction::invalid};
    KeyModifier modifiers{KeyModifier::none};
    bool repeat{false};

    friend bool operator==(const KeyboardInputEvent&, const KeyboardInputEvent&) = default;
};

enum class WindowInputAction : std::uint8_t {
    invalid,
    focus_gained,
    focus_lost,
    resized,
};

struct WindowInputEvent {
    WindowInputAction action{WindowInputAction::invalid};
    int width{0};
    int height{0};

    friend bool operator==(const WindowInputEvent&, const WindowInputEvent&) = default;
};

using PlatformInputEvent = std::variant<
    PointerInputEvent,
    KeyboardInputEvent,
    WindowInputEvent>;

[[nodiscard]] bool is_valid(const PointerIdentity& identity) noexcept;
[[nodiscard]] bool is_valid(const PointerInputEvent& event) noexcept;
[[nodiscard]] bool is_valid(const KeyboardInputEvent& event) noexcept;
[[nodiscard]] bool is_valid(const WindowInputEvent& event) noexcept;
[[nodiscard]] bool is_valid(const PlatformInputEvent& event) noexcept;

class PlatformInputBatch final {
public:
    void reserve(std::size_t capacity);

    // Returns false when a consecutive move for the same pointer replaces the
    // previous move. Invalid input is rejected before the batch is mutated.
    bool append(PlatformInputEvent event);
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::uint64_t coalesced_move_count() const noexcept;
    [[nodiscard]] std::span<const PlatformInputEvent> events() const noexcept;

private:
    std::vector<PlatformInputEvent> events_;
    std::uint64_t coalesced_move_count_{0};
};

} // namespace ryn::input
