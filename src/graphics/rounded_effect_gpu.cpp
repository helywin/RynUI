#include "graphics/rounded_effect_gpu.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::graphics {
namespace {

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] runtime::Rect scale_rect(runtime::Rect rect, float scale) noexcept {
    return {
        rect.x * scale,
        rect.y * scale,
        rect.width * scale,
        rect.height * scale,
    };
}

[[nodiscard]] float smoothstep(float edge0, float edge1, float value) noexcept {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] float gaussian_edge(float distance, float sigma) noexcept {
    if (sigma <= 0.0F) {
        return distance <= 0.0F ? 1.0F : 0.0F;
    }
    constexpr float inverse_sqrt_two = 0.7071067811865475F;
    const float value = distance * inverse_sqrt_two / sigma;
    const float sign = value < 0.0F ? -1.0F : 1.0F;
    const float absolute = std::abs(value);
    const float t = 1.0F / (1.0F + 0.3275911F * absolute);
    const float polynomial = (((((1.061405429F * t - 1.453152027F) * t)
        + 1.421413741F) * t - 0.284496736F) * t + 0.254829592F) * t;
    const float approximate_erf = sign
        * (1.0F - polynomial * std::exp(-absolute * absolute));
    return std::clamp(
        0.5F * (1.0F - approximate_erf),
        0.0F,
        1.0F);
}

[[nodiscard]] RoundedEffectKind decode_kind(float encoded) {
    if (encoded == 0.0F) return RoundedEffectKind::outer_shadow;
    if (encoded == 1.0F) return RoundedEffectKind::inset_shadow;
    if (encoded == 2.0F) return RoundedEffectKind::outline;
    throw std::invalid_argument("Rounded effect GPU kind is invalid");
}

[[nodiscard]] float encode_kind(RoundedEffectKind kind) {
    switch (kind) {
    case RoundedEffectKind::outer_shadow: return 0.0F;
    case RoundedEffectKind::inset_shadow: return 1.0F;
    case RoundedEffectKind::outline: return 2.0F;
    }
    throw std::invalid_argument("Rounded effect kind is invalid");
}

[[nodiscard]] bool contains(runtime::Rect rect, runtime::Point point) noexcept {
    return point.x >= rect.x && point.y >= rect.y
        && point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
}

} // namespace

void validate_rounded_effect_device_metrics(RoundedEffectDeviceMetrics metrics) {
    if (metrics.pixel_width == 0 || metrics.pixel_height == 0
            || !finite(metrics.display_scale) || metrics.display_scale <= 0.0F) {
        throw std::invalid_argument(
            "Rounded effect device metrics must have positive pixels and scale");
    }
}

runtime::Rect rounded_effect_logical_viewport(RoundedEffectDeviceMetrics metrics) {
    validate_rounded_effect_device_metrics(metrics);
    return {
        0.0F,
        0.0F,
        static_cast<float>(metrics.pixel_width) / metrics.display_scale,
        static_cast<float>(metrics.pixel_height) / metrics.display_scale,
    };
}

RoundedEffectGpuInstance pack_rounded_effect_instance(
    const RoundedEffectInstance& instance,
    RoundedEffectDeviceMetrics metrics) {
    validate_rounded_effect(instance);
    validate_rounded_effect_device_metrics(metrics);
    const float scale = metrics.display_scale;
    const auto logical_bounds = intersect_effect_bounds(
        rounded_effect_bounds(instance, 1.0F / scale),
        rounded_effect_logical_viewport(metrics));
    if (logical_bounds.width <= 0.0F || logical_bounds.height <= 0.0F) {
        throw std::invalid_argument("Cannot pack a fully clipped rounded effect");
    }
    const auto pixel_bounds = scale_rect(logical_bounds, scale);

    auto logical_shape = instance.geometry.shape.rect;
    logical_shape.x += instance.geometry.translation.x;
    logical_shape.y += instance.geometry.translation.y;
    const auto pixel_shape = scale_rect(logical_shape, scale);
    auto pixel_clip = runtime::Rect{
        0.0F,
        0.0F,
        static_cast<float>(metrics.pixel_width),
        static_cast<float>(metrics.pixel_height),
    };
    if (instance.geometry.ancestor_clip.has_value()) {
        pixel_clip = intersect_effect_bounds(
            pixel_clip,
            scale_rect(instance.geometry.ancestor_clip->bounds, scale));
    }

    const auto& color = instance.material.color;
    return {
        {
            -1.0F + 2.0F * pixel_bounds.x / metrics.pixel_width,
            1.0F - 2.0F * pixel_bounds.y / metrics.pixel_height,
            2.0F * pixel_bounds.width / metrics.pixel_width,
            -2.0F * pixel_bounds.height / metrics.pixel_height,
        },
        {pixel_shape.x, pixel_shape.y, pixel_shape.width, pixel_shape.height},
        {pixel_clip.x, pixel_clip.y,
         pixel_clip.x + pixel_clip.width, pixel_clip.y + pixel_clip.height},
        {color.red(), color.green(), color.blue(), color.alpha()},
        {
            instance.geometry.shape.radius * scale,
            instance.geometry.offset.x * scale,
            instance.geometry.offset.y * scale,
            instance.geometry.blur * scale,
        },
        {
            instance.geometry.spread * scale,
            instance.geometry.outline_width * scale,
            instance.geometry.outline_offset * scale,
            encode_kind(instance.geometry.kind),
        },
        {instance.material.opacity, 1.0F, 0.0F, 0.0F},
    };
}

float rounded_effect_gpu_coverage_reference(
    runtime::Point point_pixels,
    const RoundedEffectGpuInstance& instance) {
    const runtime::Rect clip{
        instance.clip_bounds[0],
        instance.clip_bounds[1],
        instance.clip_bounds[2] - instance.clip_bounds[0],
        instance.clip_bounds[3] - instance.clip_bounds[1],
    };
    if (!contains(clip, point_pixels)) {
        return 0.0F;
    }
    LogicalRoundedRect shape{
        {instance.shape_rect[0], instance.shape_rect[1],
         instance.shape_rect[2], instance.shape_rect[3]},
        instance.shadow_params[0],
    };
    const float offset_x = instance.shadow_params[1];
    const float offset_y = instance.shadow_params[2];
    const float blur = instance.shadow_params[3];
    const float spread = instance.effect_params[0];
    const float outline_width = instance.effect_params[1];
    const float outline_offset = instance.effect_params[2];
    const float antialias_width = instance.material_params[1];

    switch (decode_kind(instance.effect_params[3])) {
    case RoundedEffectKind::outer_shadow:
        shape.rect.x -= spread;
        shape.rect.y -= spread;
        shape.rect.width = std::max(0.0F, shape.rect.width + 2.0F * spread);
        shape.rect.height = std::max(0.0F, shape.rect.height + 2.0F * spread);
        if (shape.rect.width <= 0.0F || shape.rect.height <= 0.0F) return 0.0F;
        shape.radius = std::clamp(
            shape.radius + spread,
            0.0F,
            0.5F * std::min(shape.rect.width, shape.rect.height));
        shape.rect.x += offset_x;
        shape.rect.y += offset_y;
        return gaussian_edge(
            rounded_rect_signed_distance(point_pixels, shape), blur * 0.5F);
    case RoundedEffectKind::inset_shadow:
        if (rounded_rect_signed_distance(point_pixels, shape) > 0.0F) return 0.0F;
        shape.rect.x += offset_x;
        shape.rect.y += offset_y;
        return gaussian_edge(
            -rounded_rect_signed_distance(point_pixels, shape) - spread,
            blur * 0.5F);
    case RoundedEffectKind::outline: {
        const float distance = rounded_rect_signed_distance(point_pixels, shape);
        const float half_aa = antialias_width * 0.5F;
        const float inner = smoothstep(
            outline_offset - half_aa,
            outline_offset + half_aa,
            distance);
        const float outer = 1.0F - smoothstep(
            outline_offset + outline_width - half_aa,
            outline_offset + outline_width + half_aa,
            distance);
        return std::clamp(inner * outer, 0.0F, 1.0F);
    }
    }
    return 0.0F;
}

std::array<float, 4> rounded_effect_gpu_fragment_reference(
    runtime::Point point_pixels,
    const RoundedEffectGpuInstance& instance) {
    const float coverage = rounded_effect_gpu_coverage_reference(
        point_pixels, instance);
    return {
        instance.color[0],
        instance.color[1],
        instance.color[2],
        instance.color[3] * instance.material_params[0] * coverage,
    };
}

} // namespace ryn::graphics
