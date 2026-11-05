#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
CalendarThemeToken derive_calendar(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_calendar_override(CalendarThemeToken&, const CalendarTokenOverride&);
} // namespace ryn::detail
