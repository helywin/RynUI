#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
QRCodeThemeToken derive_qrcode(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_qrcode_override(QRCodeThemeToken&, const QRCodeTokenOverride&);
} // namespace ryn::detail
