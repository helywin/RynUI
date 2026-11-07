#include "renderer/common/cap_geometry_packing.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {

bool needs_cap_geometry(const graphics::RoundedEffectInstance& instance) noexcept {
    const auto* arc = std::get_if<graphics::RoundedArcPath>(&instance.geometry.path);
    return arc && arc->cap != graphics::ArcCap::Round;
}

CapGeometryGpuV1 pack_cap_geometry(const graphics::RoundedEffectInstance& instance, SceneDeviceMetrics metrics) {
    graphics::validate_rounded_effect(instance);
    validate_scene_device_metrics(metrics);
    if (!needs_cap_geometry(instance)) {
        return {};
    }
    auto rect = instance.geometry.shape.rect;
    // Preserve the established logical translation rounding before encoding.
    rect.x += instance.geometry.translation.x;
    rect.y += instance.geometry.translation.y;
    return {{std::bit_cast<std::uint32_t>(rect.x), std::bit_cast<std::uint32_t>(rect.y),
             std::bit_cast<std::uint32_t>(rect.width), std::bit_cast<std::uint32_t>(rect.height)},
            {std::bit_cast<std::uint32_t>(instance.geometry.outline_width),
             std::bit_cast<std::uint32_t>(metrics.display_scale), 0, 0}};
}

void validate_cap_geometry(const CapGeometryGpuV1& geometry) {
    const auto finite = [](std::uint32_t bits) {
        return (bits & 0x7f800000U) != 0x7f800000U;
    };
    const auto nonnegative = [&](std::uint32_t bits) {
        return finite(bits) && (!(bits & 0x80000000U) || !(bits & 0x7fffffffU));
    };
    if (!finite(geometry.logical_rect_bits[0]) || !finite(geometry.logical_rect_bits[1]) ||
        !nonnegative(geometry.logical_rect_bits[2]) ||
        (geometry.logical_rect_bits[2] != geometry.logical_rect_bits[3] &&
         ((geometry.logical_rect_bits[2] | geometry.logical_rect_bits[3]) & 0x7fffffffU)) ||
        !nonnegative(geometry.stroke_scale_bits[0]) || !finite(geometry.stroke_scale_bits[1]) ||
        (geometry.stroke_scale_bits[1] & 0x80000000U) || !(geometry.stroke_scale_bits[1] & 0x7fffffffU) ||
        geometry.stroke_scale_bits[2] || geometry.stroke_scale_bits[3]) {
        throw std::invalid_argument("Cap Geometry ABI v1 record is invalid");
    }
}

float cap_geometry_coverage_reference(runtime::Point point_pixels, const RoundedEffectGpuInstance& base,
                                      std::uint32_t cap, const CapGeometryGpuV1& geometry) {
    validate_cap_geometry(geometry);
    if (cap < 1 || cap > 2 || base.effect_params[3] != 3) {
        throw std::invalid_argument("Cap Geometry requires a non-round arc base/link");
    }
    if (point_pixels.x < base.clip_bounds[0] || point_pixels.y < base.clip_bounds[1] ||
        point_pixels.x >= base.clip_bounds[2] || point_pixels.y >= base.clip_bounds[3]) {
        return 0;
    }
    const auto& bits = geometry.logical_rect_bits;
    const runtime::Rect rect{std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]),
                             std::bit_cast<float>(bits[2]), std::bit_cast<float>(bits[3])};
    const auto scale = std::bit_cast<float>(geometry.stroke_scale_bits[1]);
    const auto aa = 1.0F / scale;
    const auto distance = graphics::arc_signed_distance(
        {point_pixels.x / scale, point_pixels.y / scale}, {rect, rect.width / 2},
        std::bit_cast<float>(geometry.stroke_scale_bits[0]),
        {base.shadow_params[1], base.shadow_params[2], static_cast<graphics::ArcCap>(cap)});
    const auto t = std::clamp((distance + aa / 2) / aa, 0.0F, 1.0F);
    return 1 - t * t * (3 - 2 * t);
}

} // namespace ryn::detail
