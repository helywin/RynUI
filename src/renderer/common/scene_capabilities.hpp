#pragma once

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
            maximum};
}

void validate_scene_capabilities(const SceneBackendCapabilities& capabilities);
void validate_scene_buffer_requirement(const SceneBackendCapabilities& capabilities, std::size_t count,
                                       std::size_t stride, bool power_of_two_growth);

} // namespace ryn::detail
