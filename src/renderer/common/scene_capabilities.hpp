#pragma once

#include "graphics/rounded_effect.hpp"
#include "renderer/common/scene_packing.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace ryn::detail {

// Accepted input limits, not a guarantee of available device memory.
struct SceneBackendCapabilities final {
    std::uint32_t logical_scene_version{};
    std::uint32_t packed_abi_version{};
    bool quads{};
    bool glyphs{};
    bool rounded_effects{};
    bool r8_sampling{};
    bool ordered_draws{};
    bool partial_uploads{};
    std::uint64_t maximum_buffer_bytes{};
    std::uint32_t maximum_texture_width{};
    std::uint32_t maximum_texture_height{};
    std::uint32_t image_abi_version{};
    bool images{};
    bool rgba8_sampling{};
    // Optional capabilities; baseline stays off. Backends opt in explicitly.
    bool effect_paints{};
    bool effect_arc_caps{};
    std::uint32_t effect_paint_abi_version{};
    bool effect_linear_fields{};
    bool effect_conic_fields{};
    bool effect_underlays{};
    std::uint32_t cap_geometry_abi_version{};
    std::uint32_t scene_composition_version{};
    std::uint32_t rounded_clip_abi_version{};
    std::uint32_t backdrop_filter_abi_version{};
    bool masked_groups{};
    bool rounded_clip{};
    bool cropped_target_coordinates{};
    bool premultiplied_targets{};
    bool target_sampling{};
    bool gaussian_backdrop{};
    std::uint32_t maximum_composition_target_width{};
    std::uint32_t maximum_composition_target_height{};
    std::uint64_t maximum_composition_bytes{};
    std::uint32_t maximum_composition_frames{};
    std::uint32_t maximum_composition_depth{};
    std::uint32_t maximum_composition_groups{};
    std::uint32_t maximum_composition_ops{};
    std::uint32_t maximum_gaussian_radius{};
    std::uint64_t maximum_gaussian_weight_bytes{};
};

[[nodiscard]] constexpr SceneBackendCapabilities baseline_scene_capabilities() noexcept {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    return {graphics::logical_scene_version,
            packed_scene_abi_version,
            true,
            true,
            true,
            true,
            true,
            true,
            maximum,
            maximum,
            maximum,
            2,
            true,
            true};
}

void validate_scene_capabilities(const SceneBackendCapabilities& capabilities);
void validate_scene_effect_requirements(const SceneBackendCapabilities& capabilities,
                                        graphics::RoundedEffectRequirements requirements);
void validate_scene_buffer_requirement(const SceneBackendCapabilities& capabilities, std::size_t count,
                                       std::size_t stride, bool power_of_two_growth);

} // namespace ryn::detail
