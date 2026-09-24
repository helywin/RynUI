#pragma once

#include "component/retained_surface_service.hpp"

#include <array>

namespace ryn::component {

inline constexpr std::size_t button_loading_segment_count = 8;
inline constexpr std::size_t button_visual_layer_count =
    2 + button_loading_segment_count;

enum class ButtonVisualLayer : std::uint8_t {
    border,
    background,
    loading_indicator,
};

[[nodiscard]] constexpr std::size_t button_loading_segment_index(
    std::size_t segment) noexcept {
    return static_cast<std::size_t>(ButtonVisualLayer::loading_indicator) + segment;
}

using ButtonVisualData =
    std::array<graphics::QuadInstance, button_visual_layer_count>;

// Internal migration aliases for Button's typed visual adapter.
using ButtonSceneService = RetainedSurfaceService;
using ButtonSceneId = RetainedSurfaceId;
using ButtonEffectData = RetainedSurfaceEffects;
using ButtonSceneDiagnostics = RetainedSurfaceDiagnostics;

} // namespace ryn::component
