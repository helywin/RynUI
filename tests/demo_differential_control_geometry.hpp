#pragma once

#include "graphics/rounded_effect.hpp"

namespace rynui::test {

// The producer calls the real factory. This adapter changes no production rule.
inline ryn::graphics::RoundedEffectInstance inside_border(ryn::runtime::Rect area, float radius, ryn::Color color,
                                                          float opacity = 1) {
    return ryn::graphics::make_outline_effect({{area.x + 1, area.y + 1, area.width - 2, area.height - 2}, radius - 1},
                                              1, 0, color, opacity);
}

inline ryn::graphics::LogicalRoundedRect border_box(const ryn::graphics::RoundedEffectInstance& effect) {
    const auto& shape = effect.geometry.shape;
    const auto width = effect.geometry.outline_width;
    return {{shape.rect.x - width, shape.rect.y - width, shape.rect.width + 2 * width, shape.rect.height + 2 * width},
            shape.radius + width};
}

inline std::array<ryn::runtime::Rect, 4> square_border(ryn::runtime::Rect area) {
    return {{{area.x, area.y, area.width, 1},
             {area.x, area.y + area.height - 1, area.width, 1},
             {area.x, area.y + 1, 1, area.height - 2},
             {area.x + area.width - 1, area.y + 1, 1, area.height - 2}}};
}

} // namespace rynui::test
