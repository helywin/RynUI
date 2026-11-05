#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
CarouselThemeToken derive_carousel(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_carousel_override(CarouselThemeToken&, const CarouselTokenOverride&);
} // namespace ryn::detail
