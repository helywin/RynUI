#include "demo_differential_control_geometry.hpp"

#include <stdexcept>

int main() {
    const ryn::runtime::Rect area{12.5F, 16.5F, 40, 24};
    for (const auto radius : {4.0F, 8.0F}) {
        const auto effect = rynui::test::inside_border(area, radius, ryn::Color{0, 0, 0, 1});
        ryn::graphics::validate_rounded_effect(effect);
        const auto canonical = rynui::test::border_box(effect);
        if (canonical.rect != area || canonical.radius != radius || effect.geometry.outline_offset != 0 ||
            effect.geometry.shape.rect.x != area.x + 1 || effect.geometry.shape.radius != radius - 1) {
            return 1;
        }
    }
    const auto quads = rynui::test::square_border(area);
    float sum{};
    for (std::size_t i = 0; i < quads.size(); ++i) {
        sum += quads[i].width * quads[i].height;
        for (std::size_t j = i + 1; j < quads.size(); ++j) {
            const auto& a = quads[i];
            const auto& b = quads[j];
            if (a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height) {
                return 2;
            }
        }
    }
    if (sum != area.width * area.height - (area.width - 2) * (area.height - 2)) {
        return 3;
    }
    try {
        static_cast<void>(ryn::graphics::make_outline_effect({area, 4}, 1, -1, ryn::Color{0, 0, 0, 1}));
        return 4;
    } catch (const std::invalid_argument&) {
    }
    return 0;
}
