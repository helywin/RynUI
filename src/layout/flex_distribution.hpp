#pragma once

#include <algorithm>
#include <cmath>
#include <span>

namespace ryn::layout {

inline constexpr float flex_distribution_epsilon = 0.0001F;

// Share the same min/max freezing and scaled shrink rule between Flex and
// retained control groups. Items retain their original basis for shrink weights.
template <typename Item> void distribute_flex_space(std::span<Item> items, float free_space) {
    const bool growing = free_space > flex_distribution_epsilon;
    float remaining = std::abs(free_space);
    for (auto& item : items) {
        item.frozen = false;
    }
    while (remaining > flex_distribution_epsilon) {
        float total_weight{};
        std::size_t last_adjustable = items.size();
        for (std::size_t index = 0; index < items.size(); ++index) {
            auto& item = items[index];
            const float capacity = growing ? item.max_main_size - item.main_size : item.main_size - item.min_main_size;
            const float weight = growing ? item.grow : item.shrink * item.base_main_size;
            if (!item.frozen && capacity > flex_distribution_epsilon && weight > 0) {
                total_weight += weight;
                last_adjustable = index;
            } else {
                item.frozen = true;
            }
        }
        if (last_adjustable == items.size() || total_weight <= 0) {
            break;
        }
        float distributed{};
        bool clamped{};
        for (auto& item : items) {
            if (item.frozen) {
                continue;
            }
            const float weight = growing ? item.grow : item.shrink * item.base_main_size;
            const float requested = remaining * weight / total_weight;
            const float capacity = growing ? item.max_main_size - item.main_size : item.main_size - item.min_main_size;
            const float delta = std::min(requested, capacity);
            item.main_size += growing ? delta : -delta;
            distributed += delta;
            if (delta + flex_distribution_epsilon < requested) {
                item.frozen = true;
                clamped = true;
            }
        }
        if (!clamped) {
            const float residual = remaining - distributed;
            if (residual > 0) {
                auto& item = items[last_adjustable];
                const float capacity =
                    growing ? item.max_main_size - item.main_size : item.main_size - item.min_main_size;
                const float delta = std::min(residual, capacity);
                item.main_size += growing ? delta : -delta;
                distributed += delta;
            }
        }
        if (distributed <= flex_distribution_epsilon) {
            break;
        }
        remaining = std::max(0.0F, remaining - distributed);
    }
}

} // namespace ryn::layout
