#pragma once
#include <ryn/slider.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
inline void validate_slider_limits(SliderLimits limits) {
    const double span = limits.maximum - limits.minimum;
    if (!std::isfinite(limits.minimum) || !std::isfinite(limits.maximum)
        || !std::isfinite(span) || span <= 0 || !std::isfinite(limits.step)
        || limits.step <= 0 || !std::isfinite(span / limits.step)
        || span / limits.step > 4503599627370496.0
        || limits.minimum + limits.step == limits.minimum
        || limits.maximum - limits.step == limits.maximum)
        throw std::invalid_argument("Slider limits require finite ordered bounds and a representable positive step");
}
inline double normalize_slider_value(double value, SliderLimits limits) {
    if (!std::isfinite(value)) throw std::invalid_argument("Slider value must be finite");
    value = std::clamp(value, limits.minimum, limits.maximum);
    const double index = std::round((value - limits.minimum) / limits.step);
    const double grid = std::clamp(std::fma(index, limits.step, limits.minimum), limits.minimum, limits.maximum);
    return limits.maximum - value <= std::abs(grid - value) ? limits.maximum : grid;
}
inline SliderRange normalize_slider_range(SliderRange value, SliderLimits limits) {
    value = {normalize_slider_value(value.lower, limits), normalize_slider_value(value.upper, limits)};
    if (value.lower > value.upper) std::swap(value.lower, value.upper);
    return value;
}
inline bool slider_inverted(SliderOrientation orientation, bool reverse) noexcept {
    return (orientation == SliderOrientation::Vertical) != reverse;
}
} // namespace ryn::detail
