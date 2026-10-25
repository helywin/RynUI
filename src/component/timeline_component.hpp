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
};

class TimelineComponentHost final : public WindowComponentParticipant {
public:
    explicit TimelineComponentHost(WindowComponentServices&);
    ~TimelineComponentHost();
    void mount(const TimelineProps&, const std::vector<TimelineItem>&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TimelineSnapshot snapshot(runtime::ComponentId) const;

private:
    void refresh(runtime::ComponentId, bool);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
