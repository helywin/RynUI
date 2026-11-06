#pragma once
#include <ryn/statistic.hpp>

namespace ryn::detail {
struct StatisticModel final {
    StatisticValue value;
    std::optional<int> precision;
    String decimal_separator{u8"."};
    String group_separator{u8","};
    StatisticFormatter formatter;
    StatisticLocale locale;
    bool explicit_decimal{true};
    bool explicit_group{true};
};

[[nodiscard]] String format_statistic(const StatisticModel& model);
void validate_statistic_locale(const StatisticLocale&);
void validate_statistic_timer(const StatisticTimer&);
[[nodiscard]] String format_statistic_timer(Duration, const String&, const StatisticLocale&);
[[nodiscard]] std::int64_t statistic_timer_quantum(const String&);
} // namespace ryn::detail
