#pragma once

#include <ryn/splitter.hpp>
#include <span>

namespace ryn::detail {

struct SplitterLimits final {
    float minimum{};
    float maximum{};
    bool collapsed{};
};

// Project desired sizes onto a finite bounded simplex. When constraints cannot
// fit, total size and nonnegative geometry take priority over min/max.
[[nodiscard]] SplitterSizes solve_splitter_sizes(float total, std::span<const float> desired,
                                                 std::span<const SplitterLimits> limits);
[[nodiscard]] SplitterSizes resize_splitter_pair(std::span<const float> sizes, std::size_t index, float desired_start,
                                                 std::span<const SplitterLimits> limits);

} // namespace ryn::detail
