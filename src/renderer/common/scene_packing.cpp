#include "renderer/common/scene_packing.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ryn::detail {
namespace {
void validate_bounds(const std::array<float, 4>& bounds) {
    if (!std::ranges::all_of(bounds, [](float value) { return std::isfinite(value); }) || bounds[2] < 0 ||
        bounds[3] < 0) {
        throw std::invalid_argument("Scene logical bounds must be finite with nonnegative extents");
    }
}

std::array<float, 4> pack_bounds(const std::array<float, 4>& bounds, runtime::Rect viewport) {
    validate_bounds(bounds);
    return {-1 + 2 * bounds[0] / viewport.width, 1 - 2 * bounds[1] / viewport.height, 2 * bounds[2] / viewport.width,
            -2 * bounds[3] / viewport.height};
}
} // namespace

QuadGpuInstance pack_quad_instance(const graphics::QuadInstance& instance, SceneDeviceMetrics metrics) {
    const auto viewport = scene_logical_viewport(metrics);
    const auto extent = std::min(instance.bounds[2], instance.bounds[3]);
    if (!std::isfinite(instance.corner_radius) || instance.corner_radius < 0) {
        throw std::invalid_argument("Scene logical corner radius must be finite and nonnegative");
    }
    return {pack_bounds(instance.bounds, viewport),
            instance.color,
            instance.opacity,
            extent > 0 ? std::clamp(instance.corner_radius / extent, 0.0F, 0.5F) : 0.0F,
            {2 * instance.translation[0] / viewport.width, -2 * instance.translation[1] / viewport.height}};
}

GlyphGpuInstance pack_glyph_instance(const graphics::GlyphInstance& instance, SceneDeviceMetrics metrics) {
    const auto viewport = scene_logical_viewport(metrics);
    auto bounds = instance.position_size;
    validate_bounds(bounds);
    const auto transform = instance.transform;
    if (!std::isfinite(transform.pivot.x) || !std::isfinite(transform.pivot.y) ||
        !std::isfinite(transform.angle_degrees)) {
        throw std::invalid_argument("Glyph transform must be finite");
    }
    const auto radians = std::remainder(static_cast<double>(transform.angle_degrees), 360.0) * std::numbers::pi / 180.0;
    const auto cosine = static_cast<float>(std::cos(radians));
    const auto sine = static_cast<float>(std::sin(radians));
    if (transform.angle_degrees != 0) {
        const auto x = bounds[0] - transform.pivot.x;
        const auto y = bounds[1] - transform.pivot.y;
        bounds[0] = transform.pivot.x + cosine * x - sine * y;
        bounds[1] = transform.pivot.y + sine * x + cosine * y;
    }
    return {pack_bounds(bounds, viewport),
            instance.uv_rect,
            {-1 + 2 * instance.clip_bounds[0] / viewport.width, 1 - 2 * instance.clip_bounds[1] / viewport.height,
             -1 + 2 * instance.clip_bounds[2] / viewport.width, 1 - 2 * instance.clip_bounds[3] / viewport.height},
            instance.color,
            {2 * instance.translation_opacity[0] / viewport.width,
             -2 * instance.translation_opacity[1] / viewport.height, instance.translation_opacity[2],
             instance.translation_opacity[3]},
            {cosine, sine * viewport.height / viewport.width, -sine * viewport.width / viewport.height, cosine}};
}

runtime::Point packed_glyph_vertex(const GlyphGpuInstance& instance, runtime::Point corner) {
    const auto x = corner.x * instance.position_size[2];
    const auto y = corner.y * instance.position_size[3];
    return {instance.position_size[0] + instance.translation_opacity[0] + x * instance.rotation_basis[0] +
                y * instance.rotation_basis[1],
            instance.position_size[1] + instance.translation_opacity[1] + x * instance.rotation_basis[2] +
                y * instance.rotation_basis[3]};
}
} // namespace ryn::detail
