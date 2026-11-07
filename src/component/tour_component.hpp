#pragma once
#include "component/window_component_services.hpp"
#include <ryn/tour.hpp>
#include <exception>

namespace ryn::detail {
struct TourSnapshot final {
    runtime::ComponentId panel;
    runtime::ComponentId content;
    runtime::ComponentId previous;
    runtime::ComponentId next;
    runtime::ComponentId close;
    std::optional<runtime::ComponentId> target;
    runtime::Rect target_bounds;
    runtime::Rect bounds;
    std::size_t current{};
    std::size_t total{};
    bool visible{};
    bool centered{};
};

class TourComponentHost final : public WindowComponentParticipant {
public:
    explicit TourComponentHost(WindowComponentServices&);
    ~TourComponentHost();
    void mount(TourProps);
    void mount_target(TourTargetRef, TourTargetContent);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TourSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_retained_translations() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId);
    void reconcile(runtime::ComponentId, std::vector<TourStep>);
    void navigate(runtime::ComponentId, int);
    void close(runtime::ComponentId, TourCloseReason);
    void deactivate(runtime::ComponentId, bool);
    void position(runtime::Size, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
    bool updating_{};
    runtime::Size viewport_;
    runtime::Rect document_clip_;
    std::optional<runtime::ComponentId> positioning_owner_;
    std::exception_ptr positioning_error_;
};
} // namespace ryn::detail
