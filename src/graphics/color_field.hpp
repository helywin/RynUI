#pragma once
#include "graphics/rounded_effect.hpp"
#include <ryn/design_token.hpp>
#include <functional>

namespace ryn::graphics {
// Tiles share one outer shape. Hard, half-open cell clips give internal sample
// boundaries one owner while retaining AA only at the field's exterior.
std::vector<RoundedEffectInstance> sampled_color_field(runtime::Rect, runtime::Point, unsigned columns, unsigned rows,
                                                       const std::function<Color(float, float)>&, EffectClip);
} // namespace ryn::graphics
