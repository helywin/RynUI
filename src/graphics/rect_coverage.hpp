#pragma once

#include "runtime/geometry.hpp"

#include <algorithm>
#include <cmath>

namespace ryn::graphics {

[[nodiscard]] inline float rect_sample_coverage(runtime::Point offset, runtime::Size extent, float width) noexcept {
    const auto overlap = [width](float center_offset, float size) {
        return std::max(0.0F, std::min({width, size, (size + width) * 0.5F - std::abs(center_offset)}));
    };
    return (overlap(offset.x, extent.width) / width) * (overlap(offset.y, extent.height) / width);
}

[[nodiscard]] inline float rect_outline_sample_coverage(runtime::Point offset, runtime::Size extent, float stroke,
                                                        float outset, float width) noexcept {
    const auto fill = [&](float margin) {
        return rect_sample_coverage(offset, {extent.width + 2 * margin, extent.height + 2 * margin}, width);
    };
    return std::clamp(fill(outset + stroke) - fill(outset), 0.0F, 1.0F);
}

} // namespace ryn::graphics
