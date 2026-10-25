#include "component/timeline_model.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace ryn::detail {
std::vector<TimelineModelEntry> timeline_order(TimelineMode mode, bool reverse,
                                               std::span<const TimelineModelItem> items) {
    if (mode != TimelineMode::Start && mode != TimelineMode::End && mode != TimelineMode::Alternate) {
        throw std::invalid_argument("Timeline mode is invalid");
    }
    std::set<std::string> keys;
    std::vector<TimelineModelEntry> result;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (item.key.empty() || !keys.insert(std::string{item.key.bytes()}).second ||
            (item.placement && *item.placement != TimelinePlacement::Start &&
             *item.placement != TimelinePlacement::End) ||
            (item.tone != TimelineTone::Default && item.tone != TimelineTone::Blue && item.tone != TimelineTone::Red &&
             item.tone != TimelineTone::Green && item.tone != TimelineTone::Gray)) {
            throw std::invalid_argument("Timeline key, placement or tone is invalid");
        }
        if (item.visible) {
            result.push_back(
                {i, item.placement.value_or(mode == TimelineMode::End || (mode == TimelineMode::Alternate && i % 2 != 0)
                                                ? TimelinePlacement::End
                                                : TimelinePlacement::Start)});
        }
    }
    if (reverse) {
        std::ranges::reverse(result);
    }
    return result;
}
} // namespace ryn::detail
