#include "animation/value.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::animation {
namespace {

void require_finite(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("animation value must be finite");
    }
}

float interpolate_scalar(float from, float to, float progress) {
    const double result = static_cast<double>(from)
        + (static_cast<double>(to) - static_cast<double>(from))
            * static_cast<double>(progress);
    const double maximum = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(result) || result < -maximum || result > maximum) {
        throw std::overflow_error("animation interpolation exceeded float range");
    }
    return static_cast<float>(result);
}

float interpolate_color_channel(float from, float to, float progress) {
    const double result = static_cast<double>(from)
        + (static_cast<double>(to) - static_cast<double>(from))
            * static_cast<double>(progress);
    if (!std::isfinite(result)) {
        throw std::overflow_error("color interpolation exceeded finite range");
    }
    return static_cast<float>(std::clamp(result, 0.0, 1.0));
}

} // namespace

AnimationValueKind value_kind(const AnimationValue& value) noexcept {
    switch (value.index()) {
    case 0:
        return AnimationValueKind::scalar;
    case 1:
        return AnimationValueKind::color;
    case 2:
        return AnimationValueKind::point;
    case 3:
        return AnimationValueKind::size;
    case 4:
        return AnimationValueKind::rect;
    case 5:
        return AnimationValueKind::logical_offset;
    default:
        return AnimationValueKind::scalar;
    }
}

void validate_animation_value(const AnimationValue& value) {
    switch (value_kind(value)) {
    case AnimationValueKind::scalar:
        require_finite(std::get<float>(value));
        return;
    case AnimationValueKind::color:
        return;
    case AnimationValueKind::point: {
        const auto point = std::get<runtime::Point>(value);
        require_finite(point.x);
        require_finite(point.y);
        return;
    }
    case AnimationValueKind::size: {
        const auto size = std::get<runtime::Size>(value);
        require_finite(size.width);
        require_finite(size.height);
        return;
    }
    case AnimationValueKind::rect: {
        const auto rect = std::get<runtime::Rect>(value);
        require_finite(rect.x);
        require_finite(rect.y);
        require_finite(rect.width);
        require_finite(rect.height);
        return;
    }
    case AnimationValueKind::logical_offset: {
        const auto offset = std::get<LogicalOffset>(value);
        require_finite(offset.x);
        require_finite(offset.y);
        return;
    }
    }
}

AnimationValue interpolate_animation_value(
    const AnimationValue& from,
    const AnimationValue& to,
    float progress) {
    validate_animation_value(from);
    validate_animation_value(to);
    require_finite(progress);
    if (from.index() != to.index()) {
        throw std::invalid_argument(
            "animation endpoints must have the same value kind");
    }
    if (progress == 0.0F) {
        return from;
    }
    if (progress == 1.0F) {
        return to;
    }

    switch (value_kind(from)) {
    case AnimationValueKind::scalar:
        return interpolate_scalar(
            std::get<float>(from), std::get<float>(to), progress);
    case AnimationValueKind::color: {
        const auto left = std::get<Color>(from);
        const auto right = std::get<Color>(to);
        return Color{
            interpolate_color_channel(left.red(), right.red(), progress),
            interpolate_color_channel(left.green(), right.green(), progress),
            interpolate_color_channel(left.blue(), right.blue(), progress),
            interpolate_color_channel(left.alpha(), right.alpha(), progress),
        };
    }
    case AnimationValueKind::point: {
        const auto left = std::get<runtime::Point>(from);
        const auto right = std::get<runtime::Point>(to);
        return runtime::Point{
            interpolate_scalar(left.x, right.x, progress),
            interpolate_scalar(left.y, right.y, progress),
        };
    }
    case AnimationValueKind::size: {
        const auto left = std::get<runtime::Size>(from);
        const auto right = std::get<runtime::Size>(to);
        return runtime::Size{
            interpolate_scalar(left.width, right.width, progress),
            interpolate_scalar(left.height, right.height, progress),
        };
    }
    case AnimationValueKind::rect: {
        const auto left = std::get<runtime::Rect>(from);
        const auto right = std::get<runtime::Rect>(to);
        return runtime::Rect{
            interpolate_scalar(left.x, right.x, progress),
            interpolate_scalar(left.y, right.y, progress),
            interpolate_scalar(left.width, right.width, progress),
            interpolate_scalar(left.height, right.height, progress),
        };
    }
    case AnimationValueKind::logical_offset: {
        const auto left = std::get<LogicalOffset>(from);
        const auto right = std::get<LogicalOffset>(to);
        return LogicalOffset{
            interpolate_scalar(left.x, right.x, progress),
            interpolate_scalar(left.y, right.y, progress),
        };
    }
    }
    throw std::invalid_argument("animation value kind is invalid");
}

} // namespace ryn::animation
