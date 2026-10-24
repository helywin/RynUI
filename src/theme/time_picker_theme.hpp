#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
TimePickerThemeToken derive_time_picker(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_time_picker_override(TimePickerThemeToken&, const TimePickerTokenOverride&);
} // namespace ryn::detail
