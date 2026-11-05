#pragma once
#include <ryn/theme.hpp>

namespace ryn::detail {
ImageThemeToken derive_image(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_image_override(ImageThemeToken&, const ImageTokenOverride&);
UploadThemeToken derive_upload(const AntDesignDefaultSeed&, const ThemeMapToken&, const ThemeAliasToken&);
void apply_upload_override(UploadThemeToken&, const UploadTokenOverride&);
} // namespace ryn::detail
