#pragma once

#include "renderer/common/rounded_effect_packing.hpp"

namespace ryn::detail {

inline constexpr std::uint32_t cap_geometry_abi_version = 1;

// Integer vertex attributes preserve all binary32 inputs, including subnormals.
// This is an instance stream, independent of the three Effect Paint tables.
struct alignas(16) CapGeometryGpuV1 final {
    std::array<std::uint32_t, 4> logical_rect_bits{};
    std::array<std::uint32_t, 4> stroke_scale_bits{};
    friend constexpr bool operator==(const CapGeometryGpuV1&, const CapGeometryGpuV1&) = default;
};

static_assert(sizeof(CapGeometryGpuV1) == 32 && alignof(CapGeometryGpuV1) == 16);
static_assert(offsetof(CapGeometryGpuV1, logical_rect_bits) == 0);
static_assert(offsetof(CapGeometryGpuV1, stroke_scale_bits) == 16);

[[nodiscard]] bool needs_cap_geometry(const graphics::RoundedEffectInstance& instance) noexcept;
[[nodiscard]] CapGeometryGpuV1 pack_cap_geometry(const graphics::RoundedEffectInstance& instance,
                                                 SceneDeviceMetrics metrics);
void validate_cap_geometry(const CapGeometryGpuV1& geometry);

// Scalar logical reference decoded from actual stream bytes. It is not an
// independent numerical oracle or a shader accuracy guarantee.
[[nodiscard]] float cap_geometry_coverage_reference(runtime::Point point_pixels, const RoundedEffectGpuInstance& base,
                                                    std::uint32_t cap, const CapGeometryGpuV1& geometry);

} // namespace ryn::detail
