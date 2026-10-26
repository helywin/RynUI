#pragma once
#include <ryn/statistic.hpp>

namespace ryn::detail {
struct StatisticModel final {
    StatisticValue value;
    std::optional<int> precision;
    String decimal_separator{u8"."};
    String group_separator{u8","};
    StatisticFormatter formatter;
};

[[nodiscard]] String format_statistic(const StatisticModel& model);
} // namespace ryn::detail
