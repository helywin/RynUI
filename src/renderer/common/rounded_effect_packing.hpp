#pragma once

#include "graphics/rounded_effect.hpp"
#include "renderer/common/scene_metrics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ryn::detail {

struct alignas(16) RoundedEffectGpuInstance final {
    std::array<float, 4> clip_rect{};
    std::array<float, 4> shape_rect{};
    std::array<float, 4> clip_bounds{};
    std::array<float, 4> color{};
    std::array<float, 4> shadow_params{};
    std::array<float, 4> effect_params{};
    std::array<float, 4> material_params{};

    friend constexpr bool operator==(const RoundedEffectGpuInstance&, const RoundedEffectGpuInstance&) = default;
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

// Valid effects outside the device clip become transparent zero-area instances,
// preserving the retained store's indices and draw order across DPI changes.
[[nodiscard]] RoundedEffectGpuInstance pack_rounded_effect_instance(const graphics::RoundedEffectInstance& instance,
                                                                    SceneDeviceMetrics metrics);

[[nodiscard]] float rounded_effect_gpu_coverage_reference(runtime::Point point_pixels,
                                                          const RoundedEffectGpuInstance& instance);

[[nodiscard]] std::array<float, 4> rounded_effect_gpu_fragment_reference(runtime::Point point_pixels,
                                                                         const RoundedEffectGpuInstance& instance);

} // namespace ryn::detail
