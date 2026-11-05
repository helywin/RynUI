#pragma once
#include "component/timeline_model.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {
struct TimelineItemSnapshot final {
    runtime::ComponentId viewport;
    runtime::ComponentId title;
    runtime::ComponentId content;
    runtime::ComponentId icon;
    TimelinePlacement placement{};
    bool visible{};
    bool loading{};
    runtime::Rect dot;
    runtime::Rect tail;
};

struct TimelineSnapshot final {
    std::vector<TimelineModelEntry> order;
    std::vector<TimelineItemSnapshot> items;
    float motion{1};
};

class TimelineComponentHost final : public WindowComponentParticipant {
public:
    explicit TimelineComponentHost(WindowComponentServices&);
    ~TimelineComponentHost();
    void mount(const TimelineProps&, const std::vector<TimelineItem>&);
    void mount(const TimelineProps&, const Prop<std::vector<TimelineItem>>&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TimelineSnapshot snapshot(runtime::ComponentId) const;

private:
    void refresh(runtime::ComponentId, bool);
    void mount_item(runtime::ComponentId, std::size_t, const std::vector<TimelineItem>&, bool);
    void bind_item(runtime::ComponentId, std::size_t);
    void update(runtime::ComponentId, std::vector<TimelineItem>);
    runtime::Size measure_horizontal(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints, float);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
