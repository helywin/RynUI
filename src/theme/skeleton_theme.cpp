#include "theme/skeleton_theme.hpp"
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
SkeletonThemeToken derive_skeleton(const AntDesignDefaultSeed& seed, const ThemeMapToken& map) {
    SkeletonThemeToken t;
    // Locked 6.6.5 Skeleton uses colorFillContent = colorFillSecondary.
    const auto base = map.color_text_base;
    const bool dark = base.red() + base.green() + base.blue() > 1.5F;
    t.colors.background = Color(base.red(), base.green(), base.blue(), dark ? .12F : .06F);
    auto& m = t.metrics;
    m.title_height = map.control_height / 2;
    m.block_radius = map.border_radius_small;
    const auto xxs = map.size_small == map.size ? map.size_xs : seed.size_unit * (seed.size_step - 3);
    m.paragraph_margin_top = map.size_large + xxs;
    m.paragraph_line_height = map.control_height / 2;
    m.title_paragraph_gap = map.control_height_small;
    m.row_gap = map.control_height / 2;
    m.avatar_gap = map.size;
    m.avatar_title_offset = map.size_small;
    m.control_height = map.control_height;
    m.control_height_small = map.control_height_small;
    m.control_height_large = map.control_height_large;
    m.control_radius = map.border_radius_small;
    m.intrinsic_width = map.control_height * 7.5F;
    return t;
}

void apply_skeleton_override(SkeletonThemeToken& t, const SkeletonTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (!value) {
            return;
        }
        if (value->is_auto()) {
            throw std::invalid_argument("Skeleton token length cannot be auto");
        }
        target = value->value();
    };
    auto& m = t.metrics;
    length(m.title_height, o.title_height);
    length(m.block_radius, o.block_radius);
    length(m.paragraph_margin_top, o.paragraph_margin_top);
    length(m.paragraph_line_height, o.paragraph_line_height);
    length(m.title_paragraph_gap, o.title_paragraph_gap);
    length(m.row_gap, o.row_gap);
    length(m.avatar_gap, o.avatar_gap);
    length(m.avatar_title_offset, o.avatar_title_offset);
    length(m.control_height, o.control_height);
    length(m.control_height_small, o.control_height_small);
    length(m.control_height_large, o.control_height_large);
    length(m.control_radius, o.control_radius);
    length(m.intrinsic_width, o.intrinsic_width);
    float extent = 0;
    for (const auto value : m.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Skeleton final metric must be finite and nonnegative");
        }
        extent += value * 2048;
    }
    if (!std::isfinite(extent)) {
        throw std::invalid_argument("Skeleton final layout extent overflows");
    }
}
} // namespace ryn::detail
