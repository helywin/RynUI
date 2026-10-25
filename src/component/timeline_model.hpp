#pragma once
#include <ryn/timeline.hpp>
#include <span>

namespace ryn::detail {
struct TimelineModelItem final {
    String key;
    std::optional<TimelinePlacement> placement;
    TimelineTone tone{TimelineTone::Default};
    bool visible{true};
};

struct TimelineModelEntry final {
    std::size_t item{};
    TimelinePlacement placement{};
    friend bool operator==(const TimelineModelEntry&, const TimelineModelEntry&) = default;
};

std::vector<TimelineModelEntry> timeline_order(TimelineMode, bool reverse, std::span<const TimelineModelItem>);
} // namespace ryn::detail
