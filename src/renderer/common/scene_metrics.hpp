#pragma once

#include "runtime/geometry.hpp"

#include <cstdint>

namespace ryn::detail {

struct SceneDeviceMetrics final {
    std::uint32_t pixel_width{};
    std::uint32_t pixel_height{};
    float display_scale{1.0F};

    friend constexpr bool operator==(SceneDeviceMetrics, SceneDeviceMetrics) = default;
};

// Zero-size/unavailable surfaces are handled by the host before scene packing.
void validate_scene_device_metrics(SceneDeviceMetrics metrics);
[[nodiscard]] runtime::Rect scene_logical_viewport(SceneDeviceMetrics metrics);

} // namespace ryn::detail
