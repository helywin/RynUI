#include "renderer/common/scene_packing.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
void validate_bounds(const std::array<float, 4> &bounds) {
    if (!std::ranges::all_of(bounds, [](float value) { return std::isfinite(value); }) ||
        bounds[2] < 0 || bounds[3] < 0)
        throw std::invalid_argument("Scene logical bounds must be finite with nonnegative extents");
}
std::array<float, 4> pack_bounds(const std::array<float, 4> &bounds, runtime::Rect viewport) {
    validate_bounds(bounds);
    return {-1 + 2 * bounds[0] / viewport.width, 1 - 2 * bounds[1] / viewport.height,
            2 * bounds[2] / viewport.width, -2 * bounds[3] / viewport.height};
}
} // namespace

QuadGpuInstance pack_quad_instance(const graphics::QuadInstance &instance,
                                   SceneDeviceMetrics metrics) {
    const auto viewport = scene_logical_viewport(metrics);
    const auto extent = std::min(instance.bounds[2], instance.bounds[3]);
    if (!std::isfinite(instance.corner_radius) || instance.corner_radius < 0)
        throw std::invalid_argument("Scene logical corner radius must be finite and nonnegative");
    return {pack_bounds(instance.bounds, viewport),
            instance.color,
            instance.opacity,
            extent > 0 ? std::clamp(instance.corner_radius / extent, 0.0F, 0.5F) : 0.0F,
            {2 * instance.translation[0] / viewport.width,
             -2 * instance.translation[1] / viewport.height}};
}

GlyphGpuInstance pack_glyph_instance(const graphics::GlyphInstance &instance,
                                     SceneDeviceMetrics metrics) {
    const auto viewport = scene_logical_viewport(metrics);
    return {pack_bounds(instance.position_size, viewport),
            instance.uv_rect,
            {-1 + 2 * instance.clip_bounds[0] / viewport.width,
             1 - 2 * instance.clip_bounds[1] / viewport.height,
             -1 + 2 * instance.clip_bounds[2] / viewport.width,
             1 - 2 * instance.clip_bounds[3] / viewport.height},
            instance.color,
            {2 * instance.translation_opacity[0] / viewport.width,
             -2 * instance.translation_opacity[1] / viewport.height,
             instance.translation_opacity[2], instance.translation_opacity[3]}};
}
} // namespace ryn::detail
