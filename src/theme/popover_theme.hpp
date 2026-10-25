#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
[[nodiscard]] PopoverThemeToken derive_popover(const AntDesignDefaultSeed&, const ThemeMapToken&,
                                               const ThemeAliasToken&);
void apply_popover_override(PopoverThemeToken&, const PopoverTokenOverride&);
} // namespace ryn::detail
