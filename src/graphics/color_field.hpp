#pragma once
#include "graphics/quad_primitive.hpp"
#include <ryn/design_token.hpp>
#include <functional>

namespace ryn::graphics {
// Logical CPU tessellation for smooth color fields. Uses the ordinary shared
// quad store/resource transaction; no texture, backend branch or new GPU ABI.
std::vector<QuadInstance> sampled_color_field(runtime::Rect, runtime::Point, unsigned columns, unsigned rows,
                                              const std::function<Color(float, float)>&);
} // namespace ryn::graphics
