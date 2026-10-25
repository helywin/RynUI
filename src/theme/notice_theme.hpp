#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
NoticeThemeToken derive_notice(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&, bool message);
void apply_notice_override(NoticeThemeToken&, const NoticeTokenOverride&);
} // namespace ryn::detail
