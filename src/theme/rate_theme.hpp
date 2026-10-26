#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
RateThemeToken derive_rate(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&, bool dark);
void apply_rate_override(RateThemeToken&, const RateTokenOverride&);
} // namespace ryn::detail
