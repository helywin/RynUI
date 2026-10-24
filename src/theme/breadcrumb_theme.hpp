#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
BreadcrumbThemeToken derive_breadcrumb(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_breadcrumb_override(BreadcrumbThemeToken&, const BreadcrumbTokenOverride&);
} // namespace ryn::detail
