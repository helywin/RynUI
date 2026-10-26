#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
TreeThemeToken derive_tree(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&,
                           const SelectThemeToken&);
void apply_tree_override(TreeThemeToken&, const TreeTokenOverride&);
} // namespace ryn::detail
