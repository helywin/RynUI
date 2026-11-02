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

QuadViewportUniform quad_viewport_uniform(std::uint32_t pixel_width, std::uint32_t pixel_height) {
    if (pixel_width == 0 || pixel_height == 0) {
        throw std::invalid_argument("Quad viewport must have positive physical extents");
    }
    return {{static_cast<float>(pixel_width), static_cast<float>(pixel_height), 0, 0}};
}

QuadVertexReference quad_gpu_vertex_reference(const QuadGpuInstance& instance, runtime::Point corner,
                                              QuadViewportUniform viewport) {
    const runtime::Size extent{std::abs(instance.clip_rect[2]) * viewport.pixel_extent[0] * 0.5F,
                               std::abs(instance.clip_rect[3]) * viewport.pixel_extent[1] * 0.5F};
    const auto guard = extent.width > 0 && extent.height > 0 ? 1.0F : 0.0F;
    const runtime::Point guard_pixels{(corner.x * 2 - 1) * guard, (corner.y * 2 - 1) * guard};
    return {{instance.clip_rect[0] + instance.translation[0] + corner.x * instance.clip_rect[2] +
                 guard_pixels.x * 2 / viewport.pixel_extent[0],
             instance.clip_rect[1] + instance.translation[1] + corner.y * instance.clip_rect[3] -
                 guard_pixels.y * 2 / viewport.pixel_extent[1]},
            {(corner.x - 0.5F) * extent.width + guard_pixels.x, (corner.y - 0.5F) * extent.height + guard_pixels.y},
            extent};
}

std::array<float, 4> quad_gpu_fragment_reference(runtime::Point point_pixels, const QuadGpuInstance& instance,
                                                 QuadViewportUniform viewport) {
    const auto first = quad_gpu_vertex_reference(instance, {0, 0}, viewport);
    const auto last = quad_gpu_vertex_reference(instance, {1, 1}, viewport);
    const auto px = [&](float ndc) {
        return (ndc + 1) * viewport.pixel_extent[0] * 0.5F;
    };
    const auto py = [&](float ndc) {
        return (1 - ndc) * viewport.pixel_extent[1] * 0.5F;
    };
    float coverage{};
    if (first.pixel_extent.width > 0 && first.pixel_extent.height > 0 && point_pixels.x >= 0 && point_pixels.y >= 0 &&
        point_pixels.x < viewport.pixel_extent[0] && point_pixels.y < viewport.pixel_extent[1] &&
        point_pixels.x >= px(first.position_ndc.x) && point_pixels.x < px(last.position_ndc.x) &&
        point_pixels.y >= py(first.position_ndc.y) && point_pixels.y < py(last.position_ndc.y)) {
        const auto center_x = (px(first.position_ndc.x) + px(last.position_ndc.x)) * 0.5F;
        const auto center_y = (py(first.position_ndc.y) + py(last.position_ndc.y)) * 0.5F;
        const auto radius = std::clamp(instance.corner_radius, 0.0F, 1.0F) *
                            std::min(first.pixel_extent.width, first.pixel_extent.height);
        const auto qx = std::abs(point_pixels.x - center_x) - (first.pixel_extent.width * 0.5F - radius);
        const auto qy = std::abs(point_pixels.y - center_y) - (first.pixel_extent.height * 0.5F - radius);
        const auto distance =
            std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F)) + std::min(std::max(qx, qy), 0.0F) - radius;
        const auto t = std::clamp(distance + 0.5F, 0.0F, 1.0F);
        coverage = 1 - t * t * (3 - 2 * t);
    }
    return {instance.color[0], instance.color[1], instance.color[2], instance.color[3] * instance.opacity * coverage};
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
