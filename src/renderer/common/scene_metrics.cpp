#include "renderer/common/scene_metrics.hpp"

#include <cmath>
#include <stdexcept>

namespace ryn::detail {

runtime::Rect scene_logical_viewport(SceneDeviceMetrics metrics) {
    if (!metrics.pixel_width || !metrics.pixel_height
        || !std::isfinite(metrics.display_scale) || metrics.display_scale <= 0) {
        throw std::invalid_argument("Scene device metrics must have positive pixels and scale");
    }
    const runtime::Rect viewport{
        0, 0, static_cast<float>(metrics.pixel_width) / metrics.display_scale,
        static_cast<float>(metrics.pixel_height) / metrics.display_scale};
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height)
        || viewport.width <= 0 || viewport.height <= 0) {
        throw std::invalid_argument("Scene logical viewport must be finite and positive");
    }
    return viewport;
}

void validate_scene_device_metrics(SceneDeviceMetrics metrics) {
    static_cast<void>(scene_logical_viewport(metrics));
}

} // namespace ryn::detail
