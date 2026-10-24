#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
StepsThemeToken derive_steps(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_steps_override(StepsThemeToken&, const StepsTokenOverride&);
} // namespace ryn::detail
