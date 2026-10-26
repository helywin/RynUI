#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
AvatarThemeToken derive_avatar(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_avatar_override(AvatarThemeToken&, const AvatarTokenOverride&);
} // namespace ryn::detail
