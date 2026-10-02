#pragma once

#include <ryn/theme.hpp>
#include <ryn/input_types.hpp>

namespace ryn::detail {
[[nodiscard]] Color semantic_status_background(const ThemeSnapshot& theme, InputStatus status);
} // namespace ryn::detail
