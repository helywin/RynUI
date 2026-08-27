#pragma once

#include <ryn/theme.hpp>

namespace ryn::detail {

// Migration adapter for internal callers that still need a process-stable
// Default snapshot. Component visuals must read their mounted ThemeScope.
using DefaultThemeSnapshot = ThemeSnapshot;

[[nodiscard]] const DefaultThemeSnapshot& default_theme_snapshot() noexcept;

} // namespace ryn::detail
