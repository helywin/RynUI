#pragma once
#include "component/window_component_services.hpp"
#include <ryn/modal.hpp>

namespace ryn::detail {
struct ModalSnapshot final {
    runtime::ComponentId overlay;
    runtime::ComponentId dialog;
    runtime::ComponentId title;
    runtime::ComponentId body_viewport;
    runtime::ComponentId body;
    runtime::ComponentId footer;
    runtime::ComponentId close;
    runtime::ComponentId ok;
    runtime::ComponentId cancel;
    runtime::Rect bounds;
    runtime::Rect body_bounds;
    float scroll{};
    float maximum_scroll{};
    bool visible{};
    bool topmost{};
};

class ModalComponentHost final : public WindowComponentParticipant {
public:
    explicit ModalComponentHost(WindowComponentServices&);
    ~ModalComponentHost();
    void mount(const ModalProps&, const ModalContent&, const ModalSlots&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] ModalSnapshot snapshot(runtime::ComponentId) const;

    [[nodiscard]] bool visible() const noexcept {
        return !stack_.empty();
    }

    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool synchronize_auxiliary_fragments() override;

private:
    struct Entry final {
        runtime::ComponentId owner;
        runtime::ComponentId overlay;
        std::optional<input::InteractionId> trigger;
        bool restore{};
    };

    struct Transition final {
        runtime::ComponentId owner;
        bool visible{};
    };

    void refresh(runtime::ComponentId, bool geometry);
    void reconcile();
    void request_ok(runtime::ComponentId);
    void request_cancel(runtime::ComponentId, ModalCancelReason);
    [[nodiscard]] runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    bool reveal_focus(runtime::ComponentId, bool caret_ready);
    bool scroll_to(runtime::ComponentId, float);
    void focus_top();
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<Entry> stack_;
    std::vector<Transition> transitions_;
    std::uint64_t next_order_{};
    bool mounting_{};
    bool reconciling_{};
    bool reconcile_again_{};
    bool notifying_{};
    bool focus_pending_{};
    std::optional<input::InteractionId> restore_target_;
};
} // namespace ryn::detail
