#pragma once

#include "component/window_component_services.hpp"
#include <ryn/segmented.hpp>

namespace ryn::detail {
struct SegmentedSnapshot final {
    String value;
    std::optional<std::size_t> selected;
    std::vector<runtime::ComponentId> items;
};

class SegmentedComponentHost final : public WindowComponentParticipant {
public:
    explicit SegmentedComponentHost(WindowComponentServices&);
    ~SegmentedComponentHost();
    void mount(const SegmentedProps&, const std::vector<SegmentedOption>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] SegmentedSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId, bool geometry);
    void activate(runtime::ComponentId, std::size_t);
    void reconcile_input(runtime::ComponentId);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    runtime::Size measure_item(runtime::ComponentId, std::size_t, layout::LayoutEngine&, layout::Constraints);
    void place_item(runtime::ComponentId, std::size_t, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
