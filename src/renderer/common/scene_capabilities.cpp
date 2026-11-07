#include "renderer/common/scene_capabilities.hpp"
#include "renderer/common/effect_paint_packing.hpp"
#include "renderer/common/cap_geometry_packing.hpp"

#include <bit>
#include <stdexcept>

namespace ryn::detail {

void validate_scene_capabilities(const SceneBackendCapabilities& value) {
    if (value.logical_scene_version != graphics::logical_scene_version) {
        throw std::invalid_argument("Renderer logical scene version is incompatible");
    }
    if (value.packed_abi_version != packed_scene_abi_version) {
        throw std::invalid_argument("Renderer packed ABI version is incompatible");
    }
    if (!value.quads) {
        throw std::invalid_argument("Renderer requires Quad support");
    }
    if (!value.glyphs) {
        throw std::invalid_argument("Renderer requires Glyph support");
    }
    if (!value.rounded_effects) {
        throw std::invalid_argument("Renderer requires RoundedEffect support");
    }
    if (!value.r8_sampling) {
        throw std::invalid_argument("Renderer requires R8 atlas sampling");
    }
    if (!value.ordered_draws) {
        throw std::invalid_argument("Renderer requires ordered draws");
    }
    if (!value.partial_uploads) {
        throw std::invalid_argument("Renderer requires partial uploads");
    }
    if (!value.maximum_buffer_bytes || !value.maximum_texture_width || !value.maximum_texture_height) {
        throw std::invalid_argument("Renderer resource input limits must be positive");
    }
}

void validate_scene_effect_requirements(const SceneBackendCapabilities& capabilities,
                                        graphics::RoundedEffectRequirements requirements) {
    if (requirements.paint && !capabilities.effect_paints) {
        throw std::invalid_argument("Backend lacks required Effect paint capability");
    }
    if (requirements.nonround_caps && !capabilities.effect_arc_caps) {
        throw std::invalid_argument("Backend lacks required Effect arc cap capability");
    }
    if ((requirements.paint || requirements.nonround_caps) &&
        capabilities.effect_paint_abi_version != effect_paint_abi_version) {
        throw std::invalid_argument("Backend Effect paint ABI is incompatible");
    }
    if (requirements.nonround_caps && capabilities.cap_geometry_abi_version != cap_geometry_abi_version) {
        throw std::invalid_argument("Backend Cap Geometry ABI is incompatible");
    }
}

void validate_scene_buffer_requirement(const SceneBackendCapabilities& value, std::size_t count, std::size_t stride,
                                       bool power_of_two_growth) {
    if (count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Scene instance count exceeds uint32_t");
    }
    auto capacity = static_cast<std::uint32_t>(count);
    if (power_of_two_growth && capacity && capacity <= (std::uint32_t{1} << 31)) {
        capacity = std::bit_ceil(capacity);
    }
    if (stride && capacity > value.maximum_buffer_bytes / stride) {
        throw std::length_error("Scene instance buffer exceeds renderer input limit");
    }
}

} // namespace ryn::detail
