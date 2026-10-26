#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
StatisticThemeToken derive_statistic(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_statistic_override(StatisticThemeToken&, const StatisticTokenOverride&);
} // namespace ryn::detail
