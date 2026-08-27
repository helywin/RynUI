#pragma once

#include "input/interaction_registry.hpp"
#include "input/platform_input.hpp"
#include "runtime/frame_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::input {

enum class FocusModality : std::uint8_t {
    pointer,
    keyboard,
};

struct FocusSnapshot final {
    std::optional<InteractionId> focused;
    FocusModality modality{FocusModality::pointer};
    bool window_active{true};
    bool focus_visible{false};
    bool keyboard_pressed{false};
};

struct FocusManagerDiagnostics final {
    std::uint64_t keyboard_events{0};
    std::uint64_t pointer_focus_requests{0};
    std::uint64_t traversals{0};
    std::uint64_t focus_changes{0};
    std::uint64_t focus_clears{0};
    std::uint64_t state_notifications{0};
    std::uint64_t keyboard_presses{0};
    std::uint64_t keyboard_press_cancels{0};
    std::uint64_t activations{0};
    std::uint64_t activation_rejections{0};
    std::uint64_t stale_clears{0};
    std::uint64_t reentrant_rejections{0};
    std::uint64_t frame_requests{0};
};

class FocusManager final {
public:
    explicit FocusManager(
        InteractionRegistry& registry,
        runtime::FrameRequestState* frames = nullptr) noexcept;

    void reserve(std::size_t focus_capacity);
    void dispatch(const KeyboardInputEvent& event);
    bool focus_from_pointer(std::optional<InteractionId> target);
    bool request_focus(InteractionId target, FocusModality modality);
    bool clear_focus();
    void set_window_active(bool active);
    void synchronize();
    void cancel_interaction(InteractionId interaction);

    [[nodiscard]] FocusSnapshot state() const;
    [[nodiscard]] const FocusManagerDiagnostics& diagnostics() const noexcept;

private:
    void begin_operation();
    void end_operation() noexcept;
    void rebuild_focus_order();
    [[nodiscard]] bool can_focus(InteractionId target) const;
    [[nodiscard]] std::optional<InteractionId> focus_candidate(
        std::optional<InteractionId> target) const;
    [[nodiscard]] bool set_focus_internal(
        std::optional<InteractionId> target,
        FocusModality modality);
    [[nodiscard]] bool clear_focus_internal(bool stale);
    [[nodiscard]] bool cancel_keyboard_press_internal(bool notify);
    void sanitize_internal();
    [[nodiscard]] bool activation_permitted(InteractionId target);
    void activate(InteractionId target);
    void notify_state(InteractionId target, FocusPresentation presentation);
    void request_frame() noexcept;
    [[nodiscard]] FocusPresentation presentation_for(
        InteractionId target) const noexcept;

    InteractionRegistry* registry_;
    runtime::FrameRequestState* frames_;
    std::vector<InteractionId> focus_order_;
    std::optional<InteractionId> focused_;
    std::optional<InteractionId> keyboard_press_;
    FocusModality modality_{FocusModality::pointer};
    bool window_active_{true};
    FocusManagerDiagnostics diagnostics_;
    bool dispatching_{false};
    bool frame_requested_during_dispatch_{false};
};

} // namespace ryn::input
