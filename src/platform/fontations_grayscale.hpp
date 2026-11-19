#pragma once

#include "font/font_runtime.hpp"

namespace ryn::detail {

[[nodiscard]] std::shared_ptr<font::PlatformFontRasterizer> make_fontations_grayscale_rasterizer();

} // namespace ryn::detail
