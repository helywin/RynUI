#pragma once

#include "input/interaction_registry.hpp"
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {
class WindowComponentServices;
class WindowOverlayStack;
struct WindowOverlayLifetime;
struct WindowOverlayCompanionState;

enum class WindowOverlayKind { Modal, Drawer, FullscreenSpin };

// Window identity and exact owner generation travel together, including for explicit service parents.
class WindowOverlayTicket final {
public:
    friend bool operator==(const WindowOverlayTicket&, const WindowOverlayTicket&) noexcept;

private:
    friend class WindowOverlayStack;
    input::InputRootRef owner_;
    std::weak_ptr<WindowOverlayLifetime> lifetime_;
    std::uint64_t id_{};
};

struct WindowOverlayState final {
    bool input_visible{};
    bool paint_visible{};
    bool focus_trap{true};
    bool restore_focus{true};
    std::optional<int> layer_priority{1000};
    std::optional<WindowOverlayTicket> parent;
    runtime::ComponentId body;
    runtime::ComponentId primary_action;
    std::optional<input::InteractionId> fallback_focus;
    friend bool operator==(const WindowOverlayState&, const WindowOverlayState&) = default;
};

struct WindowOverlayView final {
    runtime::ComponentId owner;
    runtime::ComponentId root;
    WindowOverlayKind kind{};
    std::optional<runtime::ComponentId> parent;
    std::uint64_t opening_order{};
    std::uint64_t paint_rank{};
    friend bool operator==(const WindowOverlayView&, const WindowOverlayView&) = default;
};

class WindowOverlayCompanion final {
public:
    [[nodiscard]] bool bound() const;
    [[nodiscard]] std::optional<std::uint64_t> stack_order() const;
    bool set_active(bool);
    bool update(bool, std::span<const input::InputConstraintUpdate>);
    bool release() noexcept;

private:
    friend class WindowOverlayStack;
    std::shared_ptr<WindowOverlayCompanionState> state_;
};

// Private window service for Modal, Drawer and fullscreen Spin. The same committed forest supplies
// painter ranks, the single owned Restrict authority, companion grants and one-shot focus recovery.
class WindowOverlayStack final {
public:
    explicit WindowOverlayStack(WindowComponentServices&);
    ~WindowOverlayStack();
    WindowOverlayStack(const WindowOverlayStack&) = delete;
    WindowOverlayStack& operator=(const WindowOverlayStack&) = delete;

    [[nodiscard]] WindowOverlayTicket add(input::InputRootRef owner, input::InputRootRef root, WindowOverlayKind);
    void update(const WindowOverlayTicket&, const WindowOverlayState&);
    bool reconcile(bool quiet = false);
    void on_destroy() noexcept;
    void dispose() noexcept;
    void on_window_active(bool);
    void synchronize_focus();

    [[nodiscard]] std::optional<WindowOverlayView> top() const noexcept;
    [[nodiscard]] bool is_top(runtime::ComponentId) const noexcept;
    [[nodiscard]] std::span<const WindowOverlayView> ordered() const noexcept;
    [[nodiscard]] std::optional<runtime::ComponentId> input_domain() const noexcept;
    [[nodiscard]] std::optional<runtime::ComponentId> drawer_parent(runtime::ComponentId) const noexcept;
    [[nodiscard]] bool logical_branch_active(runtime::ComponentId) const;
    [[nodiscard]] WindowOverlayCompanion register_tour_controls(input::InputRootRef, std::vector<input::InputRootRef>,
                                                                bool);

private:
    friend class WindowOverlayCompanion;

    struct Entry final {
        WindowOverlayTicket ticket;
        input::InputRootRef root;
        WindowOverlayKind kind{};
        WindowOverlayState desired;
        std::optional<input::InteractionId> trigger;
        std::optional<input::InteractionId> last_focus;
        std::uint64_t order{};
        bool published{};
    };

    [[nodiscard]] Entry* find(const WindowOverlayTicket&) noexcept;
    [[nodiscard]] bool candidate(const Entry&) const;
    [[nodiscard]] bool companion_permitted(const WindowOverlayCompanionState&, std::optional<runtime::ComponentId>,
                                           bool) const;
    [[nodiscard]] std::optional<std::uint64_t> companion_order() const noexcept;
    void retire_companion(WindowOverlayCompanionState*) noexcept;
    bool update_companion(WindowOverlayCompanionState&, bool, std::span<const input::InputConstraintUpdate>);
    WindowComponentServices* services_;
    std::shared_ptr<WindowOverlayLifetime> lifetime_;
    std::vector<Entry> entries_;
    std::vector<WindowOverlayView> published_;
    std::vector<std::shared_ptr<WindowOverlayCompanionState>> companions_;
    input::InputConstraintLease input_lease_;
    input::InputConstraintAuthority input_authority_;
    std::optional<runtime::ComponentId> input_domain_;
    std::optional<input::InteractionId> restore_target_;
    std::uint64_t next_id_{};
    std::uint64_t next_order_{};
    std::uint64_t top_epoch_{};
    std::uint64_t focus_epoch_{};
    std::uint64_t focus_ticket_{};
    bool dirty_{};
    bool focus_pending_{};
};
} // namespace ryn::detail
