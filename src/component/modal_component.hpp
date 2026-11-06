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
    void update_overlay(runtime::ComponentId);

    struct Transition final {
        runtime::ComponentId owner;
        bool visible{};
    };

    void refresh(runtime::ComponentId, bool geometry);
    void mount_panel(const ModalProps&, const ModalContent&, const ModalSlots&, const DrawerProps*,
                     const DrawerSlots* = nullptr);
    void reconcile_drawer_content(runtime::ComponentId);
    void resize_drawer(runtime::ComponentId, float, bool);
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
    bool mounting_{};
    bool reconciling_{};
    bool reconcile_again_{};
    bool notifying_{};
};
} // namespace ryn::detail
