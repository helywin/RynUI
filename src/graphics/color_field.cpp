#include "graphics/color_field.hpp"
#include <algorithm>
#include <stdexcept>

namespace ryn::graphics {
std::vector<RoundedEffectInstance> sampled_color_field(runtime::Rect b, runtime::Point translation, unsigned columns,
                                                       unsigned rows, const std::function<Color(float, float)>& sample,
                                                       EffectClip clip) {
    if (!columns || !rows || columns > 256 || rows > 256 || columns * rows > 4096) {
        throw std::invalid_argument("Logical color field sample count is invalid");
    }
    std::vector<RoundedEffectInstance> effects;
    effects.reserve(columns * rows);
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < columns; ++x) {
            const float u = columns == 1 ? 0 : float(x) / float(columns - 1);
            const float v = rows == 1 ? 0 : float(y) / float(rows - 1);
            const auto color = sample(u, v);
            const float left = x == 0 ? clip.bounds.x : b.x + translation.x + b.width * float(x) / float(columns);
            const float top = y == 0 ? clip.bounds.y : b.y + translation.y + b.height * float(y) / float(rows);
            const float right = x + 1 == columns ? clip.bounds.x + clip.bounds.width
                                                 : b.x + translation.x + b.width * float(x + 1) / float(columns);
            const float bottom = y + 1 == rows ? clip.bounds.y + clip.bounds.height
                                               : b.y + translation.y + b.height * float(y + 1) / float(rows);
            const auto cell = intersect_effect_bounds(
                {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)}, clip.bounds);
            effects.push_back(make_shadow_effect({b, 0}, ShadowLayer{ShadowKind::outer, {}, 0, 0, color}, translation,
                                                 EffectClip{clip.identity, cell}));
        }
    }
    return effects;
}
} // namespace ryn::graphics
