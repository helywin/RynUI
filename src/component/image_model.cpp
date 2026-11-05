#include "component/image_model.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ryn::detail {
void validate_image_preview(ImagePreviewOptions o) {
    if (!std::isfinite(o.min_scale) || !std::isfinite(o.max_scale) || !std::isfinite(o.scale_step) ||
        o.min_scale <= 0 || o.max_scale < o.min_scale || o.max_scale > 1000 || o.scale_step <= 0 ||
        o.scale_step > 100) {
        throw std::invalid_argument("Invalid Image preview scale range/step");
    }
}

runtime::Rect fit_image(runtime::Rect box, runtime::Size size, ImageFit fit) {
    if (fit < ImageFit::Contain || fit > ImageFit::ScaleDown || !std::isfinite(size.width) ||
        !std::isfinite(size.height) || size.width <= 0 || size.height <= 0) {
        throw std::invalid_argument("Invalid Image fit/source size");
    }
    if (box.width <= 0 || box.height <= 0) {
        return {box.x, box.y, 0, 0};
    }
    if (fit == ImageFit::Fill) {
        return box;
    }
    float scale = fit == ImageFit::Cover ? std::max(box.width / size.width, box.height / size.height)
                                         : std::min(box.width / size.width, box.height / size.height);
    if (fit == ImageFit::None) {
        scale = 1;
    }
    if (fit == ImageFit::ScaleDown) {
        scale = std::min(1.F, scale);
    }
    const auto w = size.width * scale;
    const auto h = size.height * scale;
    return {box.x + (box.width - w) / 2, box.y + (box.height - h) / 2, w, h};
}

ImageTransform clamp_image_transform(ImageTransform t, runtime::Size image, runtime::Size viewport,
                                     ImagePreviewOptions o) {
    validate_image_preview(o);
    if (!std::isfinite(t.scale) || !std::isfinite(t.rotate) || !std::isfinite(t.x) || !std::isfinite(t.y)) {
        throw std::invalid_argument("Non-finite Image transform");
    }
    t.scale = std::clamp(t.scale, o.min_scale, o.max_scale);
    t.rotate = std::remainder(t.rotate, 360.F);
    const auto angle = t.rotate * std::numbers::pi_v<float> / 180;
    const auto w = (std::abs(std::cos(angle)) * image.width + std::abs(std::sin(angle)) * image.height) * t.scale;
    const auto h = (std::abs(std::sin(angle)) * image.width + std::abs(std::cos(angle)) * image.height) * t.scale;
    const float dx = o.movable ? std::max(0.F, (w - viewport.width) / 2) : 0;
    const float dy = o.movable ? std::max(0.F, (h - viewport.height) / 2) : 0;
    t.x = std::clamp(t.x, -dx, dx);
    t.y = std::clamp(t.y, -dy, dy);
    return t;
}

ImageTransform image_transform_action(ImageTransform t, ImageAction a, runtime::Size image, runtime::Size viewport,
                                      ImagePreviewOptions o) {
    switch (a) {
    case ImageAction::ZoomIn:
        t.scale *= 1 + o.scale_step;
        break;
    case ImageAction::ZoomOut:
        t.scale /= 1 + o.scale_step;
        break;
    case ImageAction::RotateLeft:
        t.rotate -= 90;
        break;
    case ImageAction::RotateRight:
        t.rotate += 90;
        break;
    case ImageAction::FlipX:
        t.flip_x = !t.flip_x;
        break;
    case ImageAction::FlipY:
        t.flip_y = !t.flip_y;
        break;
    case ImageAction::Reset:
        t = {};
        t.scale = o.min_scale;
        break;
    default:
        break;
    }
    return clamp_image_transform(t, image, viewport, o);
}
} // namespace ryn::detail
