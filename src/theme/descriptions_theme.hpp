#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
DescriptionsThemeToken derive_descriptions(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&,
                                           std::span<const ThemeAlgorithm>);
void apply_descriptions_override(DescriptionsThemeToken&, const DescriptionsTokenOverride&);
} // namespace ryn::detail
