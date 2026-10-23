#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
SegmentedThemeToken derive_segmented(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&,
                                     std::span<const ThemeAlgorithm>);
void apply_segmented_override(SegmentedThemeToken&, const SegmentedTokenOverride&);
} // namespace ryn::detail
