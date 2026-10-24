#pragma once
#include "component/window_component_services.hpp"
#include <ryn/steps.hpp>

namespace ryn::detail {
struct StepsSnapshot final {
    std::size_t current{};
    std::vector<runtime::ComponentId> items;
    std::vector<StepsStatus> statuses;
    std::vector<runtime::NodeId> icons;
};

class StepsComponentHost final : public WindowComponentParticipant {
public:
    explicit StepsComponentHost(WindowComponentServices&);
    ~StepsComponentHost();
    void mount(const StepsProps&, const std::vector<StepsItem>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] StepsSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void on_input_boundary_changed() override;
    void on_window_active(bool) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId, bool);
    void reconcile(runtime::ComponentId);
    void activate(runtime::ComponentId, std::size_t);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    runtime::Size measure_item(runtime::ComponentId, std::size_t, layout::LayoutEngine&, layout::Constraints);
    void place_item(runtime::ComponentId, std::size_t, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
