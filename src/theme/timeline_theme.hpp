#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
TimelineThemeToken derive_timeline(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_timeline_override(TimelineThemeToken&, const TimelineTokenOverride&);
} // namespace ryn::detail
