#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
FormThemeToken derive_form(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_form_override(FormThemeToken&, const FormTokenOverride&);
} // namespace ryn::detail
