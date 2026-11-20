#pragma once

#include "renderer/sdl/locked_shape_coverage.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace rynui::example {

struct ShapeBlendSource {
    ryn::detail::LockedShapeGeometry caster;
    ryn::detail::LockedShapeGeometry fill;
    ryn::detail::LockedShapeGeometry border;
    std::array<std::uint8_t, 3> shadow_color;
    std::array<std::uint8_t, 3> fill_color;
    std::array<std::uint8_t, 3> border_color;
    std::uint32_t width;
    std::uint32_t height;
};

struct ShapeBlendSample {
    std::uint32_t x;
    std::uint32_t y;
    std::array<std::uint8_t, 3> web;
    std::array<std::uint8_t, 3> native;
};

// Reconstruct RGB8 source-over on white independently for the two raster paths.
[[nodiscard]] std::vector<ShapeBlendSample> shape_blend_samples(const ShapeBlendSource& source);

} // namespace rynui::example
