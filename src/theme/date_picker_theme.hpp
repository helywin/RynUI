#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
DatePickerThemeToken derive_date_picker(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_date_picker_override(DatePickerThemeToken&, const DatePickerTokenOverride&);
} // namespace ryn::detail
