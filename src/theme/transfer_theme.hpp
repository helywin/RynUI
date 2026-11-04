#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
TransferThemeToken derive_transfer(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_transfer_override(TransferThemeToken&, const TransferTokenOverride&);
} // namespace ryn::detail
