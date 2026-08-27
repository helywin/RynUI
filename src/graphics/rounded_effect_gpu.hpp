#pragma once

#include "graphics/rounded_effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ryn::graphics {

struct RoundedEffectDeviceMetrics final {
    std::uint32_t pixel_width{};
    std::uint32_t pixel_height{};
    float display_scale{1.0F};

    friend constexpr bool operator==(
        RoundedEffectDeviceMetrics,
        RoundedEffectDeviceMetrics) = default;
};

struct alignas(16) RoundedEffectGpuInstance final {
    std::array<float, 4> clip_rect{};
    std::array<float, 4> shape_rect{};
    std::array<float, 4> clip_bounds{};
    std::array<float, 4> color{};
    std::array<float, 4> shadow_params{};
    std::array<float, 4> effect_params{};
    std::array<float, 4> material_params{};

    friend constexpr bool operator==(
        const RoundedEffectGpuInstance&,
        const RoundedEffectGpuInstance&) = default;
};

static_assert(sizeof(RoundedEffectGpuInstance) == 112);
static_assert(offsetof(RoundedEffectGpuInstance, clip_rect) == 0);
static_assert(offsetof(RoundedEffectGpuInstance, shape_rect) == 16);
static_assert(offsetof(RoundedEffectGpuInstance, clip_bounds) == 32);
static_assert(offsetof(RoundedEffectGpuInstance, color) == 48);
static_assert(offsetof(RoundedEffectGpuInstance, shadow_params) == 64);
static_assert(offsetof(RoundedEffectGpuInstance, effect_params) == 80);
static_assert(offsetof(RoundedEffectGpuInstance, material_params) == 96);

inline constexpr std::uint32_t rounded_effect_vertex_count = 6;

void validate_rounded_effect_device_metrics(RoundedEffectDeviceMetrics metrics);

[[nodiscard]] runtime::Rect rounded_effect_logical_viewport(
    RoundedEffectDeviceMetrics metrics);

[[nodiscard]] RoundedEffectGpuInstance pack_rounded_effect_instance(
    const RoundedEffectInstance& instance,
    RoundedEffectDeviceMetrics metrics);

[[nodiscard]] float rounded_effect_gpu_coverage_reference(
    runtime::Point point_pixels,
    const RoundedEffectGpuInstance& instance);

[[nodiscard]] std::array<float, 4> rounded_effect_gpu_fragment_reference(
    runtime::Point point_pixels,
    const RoundedEffectGpuInstance& instance);

} // namespace ryn::graphics
