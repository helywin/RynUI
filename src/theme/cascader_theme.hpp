#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
CascaderThemeToken derive_cascader(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_cascader_override(CascaderThemeToken&, const CascaderTokenOverride&);
} // namespace ryn::detail
