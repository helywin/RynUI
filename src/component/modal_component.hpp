#pragma once
#include "component/window_component_services.hpp"
#include "component/window_overlay_stack.hpp"
#include <ryn/modal.hpp>
#include <ryn/drawer.hpp>

namespace ryn::detail {
using ModalInputCompanion = WindowOverlayCompanion;

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
    bool drawer{};
    runtime::ComponentId extra;
    runtime::ComponentId placeholder;
    runtime::ComponentId resize_handle;
    bool loading{};
    bool body_mounted{};
    bool resizing{};
    bool paint_visible{};
    bool transitioning{};
    float progress{};
    std::uint64_t motion_revision{};
    bool dragging{};
};

class ModalComponentHost final : public WindowComponentParticipant {
public:
    explicit ModalComponentHost(WindowComponentServices&);
    ~ModalComponentHost();
    void mount(const ModalProps&, const ModalContent&, const ModalSlots&);
    void mount(const DrawerProps&, const DrawerContent&, const DrawerSlots&);
    // Internal Tour protocol: authorize only registered controls owned by this
    // exact grantee, never its external target or a common window ancestor.
    [[nodiscard]] ModalInputCompanion
    register_tour_controls(input::InputRootRef owner, std::vector<input::InputRootRef> controls, bool active = false);
    [[nodiscard]] std::optional<runtime::ComponentId> effective_input_domain() const noexcept;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] ModalSnapshot snapshot(runtime::ComponentId) const;

    [[nodiscard]] bool visible() const noexcept;
    [[nodiscard]] std::function<void()> detach_action(runtime::ComponentId) noexcept;
    void arm_service_panel_close(runtime::ComponentId);
    [[nodiscard]] bool presentation_enabled(runtime::ComponentId) const;
    void append_service_body(runtime::ComponentId, runtime::ComponentId, const Content&,
                             const runtime::CapturedComponentContext&);
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_motion() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool synchronize_auxiliary_fragments() override;

private:
    void update_overlay(runtime::ComponentId);
    void sample_drawers(animation::AnimationTime);
    void retarget_drawer(runtime::ComponentId, bool, bool suppress = false);
    void complete_drawers();

    struct Transition final {
        runtime::ComponentId owner;
        bool visible{};
    };

    void refresh(runtime::ComponentId, bool geometry);
    void mount_panel(const ModalProps&, const ModalContent&, const ModalSlots&, const DrawerProps*,
                     const DrawerSlots* = nullptr);
    void reconcile_drawer_content(runtime::ComponentId);
    void resize_drawer(runtime::ComponentId, float, bool);
    void move_modal(runtime::ComponentId, runtime::Point);
    void resize_modal(runtime::ComponentId, runtime::Size);
    [[nodiscard]] runtime::Size measure_drawer(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place_drawer(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    void reconcile();
    void request_ok(runtime::ComponentId);
    void request_cancel(runtime::ComponentId, ModalCancelReason);
    [[nodiscard]] runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    bool reveal_focus(runtime::ComponentId, bool caret_ready);
    bool scroll_to(runtime::ComponentId, float);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::shared_ptr<int> lifetime_{std::make_shared<int>(0)};
    std::vector<Transition> transitions_;
    animation::MonotonicTimeCursor drawer_clock_;
    bool mounting_{};
    bool reconciling_{};
    bool reconcile_again_{};
    bool notifying_{};
};
} // namespace ryn::detail
