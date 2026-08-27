#pragma once

#include "input/interaction_registry.hpp"
#include "input/platform_input.hpp"
#include "runtime/frame_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::input {

class FocusManager;

enum class PointerEventKind : std::uint8_t {
    move,
    down,
    up,
    cancel,
    enter,
    leave,
};

enum class PointerPropagationPhase : std::uint8_t {
    capture,
    target,
    bubble,
};

class PointerRouter;

class PointerDispatchContext final {
public:
    [[nodiscard]] const PointerInputEvent& event() const noexcept;
    [[nodiscard]] PointerEventKind kind() const noexcept;
    [[nodiscard]] PointerPropagationPhase phase() const noexcept;
    [[nodiscard]] InteractionId current_target() const noexcept;
    [[nodiscard]] InteractionId dispatch_target() const noexcept;
    [[nodiscard]] std::optional<InteractionId> actual_hit_target() const noexcept;
    [[nodiscard]] std::optional<InteractionId> press_origin() const noexcept;
    [[nodiscard]] bool propagation_stopped() const noexcept;

    void stop_propagation() noexcept;
    [[nodiscard]] bool capture_pointer();
    [[nodiscard]] bool release_pointer_capture();

private:
    friend class PointerRouter;

    PointerDispatchContext(
        PointerRouter& router,
        const PointerInputEvent& event,
        PointerEventKind kind,
        InteractionId dispatch_target,
        std::optional<InteractionId> actual_hit_target,
        std::optional<InteractionId> press_origin) noexcept;

    PointerRouter* router_;
    const PointerInputEvent* event_;
    PointerEventKind kind_;
    PointerPropagationPhase phase_{PointerPropagationPhase::target};
    InteractionId current_target_;
    InteractionId dispatch_target_;
    std::optional<InteractionId> actual_hit_target_;
    std::optional<InteractionId> press_origin_;
    bool propagation_stopped_{false};
};

struct PointerStateSnapshot final {
    PointerIdentity pointer;
    runtime::Point position;
    bool primary_down{false};
    std::optional<InteractionId> hover_target;
    std::optional<InteractionId> capture;
    std::optional<InteractionId> press_origin;
};

struct PointerRouterDiagnostics final {
    std::uint64_t input_events{0};
    std::uint64_t routes_dispatched{0};
    std::uint64_t route_entries{0};
    std::uint64_t handlers_invoked{0};
    std::uint64_t hover_enters{0};
    std::uint64_t hover_leaves{0};
    std::uint64_t captures_started{0};
    std::uint64_t captures_released{0};
    std::uint64_t cancels{0};
    std::uint64_t stale_skips{0};
    std::uint64_t reentrant_rejections{0};
    std::uint64_t frame_requests{0};
};

class PointerRouter final {
public:
    PointerRouter(
        InteractionRegistry& registry,
        HitTestSnapshot& hit_test,
        runtime::FrameRequestState* frames = nullptr,
        FocusManager* focus = nullptr) noexcept;

    void reserve(std::size_t pointer_capacity, std::size_t route_capacity);
    void dispatch(const PointerInputEvent& event);
    void cancel_all();
    void cancel_interaction(InteractionId interaction);

    [[nodiscard]] std::optional<PointerStateSnapshot> state(
        PointerIdentity pointer) const;
    [[nodiscard]] std::size_t pointer_count() const;
    [[nodiscard]] const PointerRouterDiagnostics& diagnostics() const noexcept;

private:
    friend class PointerDispatchContext;

    struct PointerState final {
        PointerIdentity pointer;
        runtime::Point position;
        bool primary_down{false};
        std::vector<InteractionId> hover_path;
        std::optional<InteractionId> capture;
        std::optional<InteractionId> press_origin;
    };

    [[nodiscard]] PointerState* find_state(PointerIdentity pointer) noexcept;
    [[nodiscard]] const PointerState* find_state(PointerIdentity pointer) const noexcept;
    [[nodiscard]] PointerState& state_for(PointerIdentity pointer);
    [[nodiscard]] bool build_route(
        InteractionId target,
        std::vector<InteractionId>& route);
    void update_hover(
        PointerState& state,
        std::optional<InteractionId> actual_target,
        const PointerInputEvent& event);
    void clear_hover(PointerState& state, const PointerInputEvent& event);
    void dispatch_route(
        PointerState& state,
        const PointerInputEvent& event,
        PointerEventKind kind,
        InteractionId target,
        std::optional<InteractionId> actual_target);
    void invoke_direct(
        PointerState& state,
        const PointerInputEvent& event,
        PointerEventKind kind,
        InteractionId target,
        std::optional<InteractionId> actual_target);
    bool invoke_handler(
        PointerState& state,
        PointerDispatchContext& context,
        InteractionId current,
        PointerPropagationPhase phase);
    void sanitize_before_dispatch(
        PointerState& state,
        const PointerInputEvent& event);
    void prune_invalid_state(PointerState& state);
    void clear_primary_state(PointerState& state, bool count_cancel);
    void abort_pointer(PointerState& state) noexcept;
    void request_frame() noexcept;
    [[nodiscard]] bool request_capture(
        const PointerDispatchContext& context);
    [[nodiscard]] bool release_capture(
        const PointerDispatchContext& context);
    [[nodiscard]] static PointerEventKind event_kind(PointerAction action);

    InteractionRegistry* registry_;
    HitTestSnapshot* hit_test_;
    runtime::FrameRequestState* frames_;
    FocusManager* focus_;
    std::vector<PointerState> states_;
    std::vector<InteractionId> route_scratch_;
    std::vector<InteractionId> next_hover_scratch_;
    std::vector<InteractionId> previous_hover_scratch_;
    std::size_t route_capacity_{0};
    PointerRouterDiagnostics diagnostics_;
    bool dispatching_{false};
    bool frame_requested_during_dispatch_{false};
};

} // namespace ryn::input
