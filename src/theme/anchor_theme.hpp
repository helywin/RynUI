#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
AnchorThemeToken derive_anchor(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_anchor_override(AnchorThemeToken&, const AnchorTokenOverride&);
} // namespace ryn::detail
