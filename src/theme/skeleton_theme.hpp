#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
[[nodiscard]] SkeletonThemeToken derive_skeleton(const AntDesignDefaultSeed&, const ThemeMapToken&);
void apply_skeleton_override(SkeletonThemeToken&, const SkeletonTokenOverride&);
} // namespace ryn::detail
