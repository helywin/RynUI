#pragma once

#include "font/font_runtime.hpp"

namespace ryn::detail {

[[nodiscard]] bool has_bundled_svg_glyph(std::uint32_t glyph) noexcept;

// The glyph index is the bundled face's actual cmap result. Origin fractions
// are relative to the top-left of the physical viewBox.
[[nodiscard]] font::FontError rasterize_bundled_svg_glyph(std::uint32_t glyph, float physical_em, float phase_x,
                                                          float phase_y, font::GlyphBitmap& result);

} // namespace ryn::detail
