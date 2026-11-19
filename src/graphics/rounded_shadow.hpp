#pragma once

#include "graphics/rounded_effect.hpp"

namespace ryn::graphics {

// Coverage of the finite, shifted shadow shape before the caster knockout.
[[nodiscard]] float rounded_shadow_gaussian(runtime::Point point, LogicalRoundedRect shape, float sigma,
                                            float antialias_width) noexcept;

[[nodiscard]] float rounded_shadow_device_box(runtime::Point point, LogicalRoundedRect shape, float sigma,
                                              float pixel_width) noexcept;

} // namespace ryn::graphics
