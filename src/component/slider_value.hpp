#pragma once
#include <ryn/slider.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <span>

namespace ryn::detail {
inline void validate_slider_hint(const SliderHintOptions& hint) {
    if (hint.mode < SliderHintMode::Auto || hint.mode > SliderHintMode::Hidden ||
        hint.placement < TooltipPlacement::Top || hint.placement > TooltipPlacement::RightBottom) {
        throw std::invalid_argument("Slider hint options are invalid");
    }
}

inline void validate_slider_limits(SliderLimits limits) {
    const double span = limits.maximum - limits.minimum;
    if (!std::isfinite(limits.minimum) || !std::isfinite(limits.maximum) || !std::isfinite(span) || span <= 0 ||
        !std::isfinite(limits.step) || limits.step <= 0 || !std::isfinite(span / limits.step) ||
        span / limits.step > 4503599627370496.0 || limits.minimum + limits.step == limits.minimum ||
        limits.maximum - limits.step == limits.maximum) {
        throw std::invalid_argument("Slider limits require finite ordered bounds and a representable positive step");
    }
}

inline void validate_slider_marks(std::span<const SliderMark> marks, SliderLimits limits) {
    validate_slider_limits(limits);
    if (marks.size() > 4096) {
        throw std::invalid_argument("Slider supports at most 4096 marks");
    }
    for (std::size_t i = 0; i < marks.size(); ++i) {
        if (!std::isfinite(marks[i].value) || marks[i].value < limits.minimum || marks[i].value > limits.maximum ||
            (i != 0 && marks[i - 1].value >= marks[i].value)) {
            throw std::invalid_argument("Slider marks must be finite, unique, sorted and inside limits");
        }
    }
}

inline SliderMarks sorted_slider_marks(SliderMarks marks, SliderLimits limits) {
    if (marks.size() > 4096) {
        throw std::invalid_argument("Slider supports at most 4096 marks");
    }
    for (const auto& mark : marks) {
        if (!std::isfinite(mark.value) || mark.value < limits.minimum || mark.value > limits.maximum) {
            throw std::invalid_argument("Slider marks must be finite and inside limits");
        }
    }
    std::ranges::sort(marks, {}, &SliderMark::value);
    validate_slider_marks(marks, limits);
    return marks;
}

inline std::vector<double> slider_visual_points(SliderLimits limits, std::span<const SliderMark> marks, bool marks_only,
                                                bool dots) {
    std::vector<double> points;
    if (dots && !marks_only) {
        const auto steps = std::floor((limits.maximum - limits.minimum) / limits.step);
        if (steps >= 4096) {
            throw std::invalid_argument("Slider dots exceed 4096 visual points");
        }
        points.reserve(static_cast<std::size_t>(steps) + marks.size() + 2);
        for (std::size_t i = 0; i <= static_cast<std::size_t>(steps); ++i) {
            points.push_back(std::clamp(std::fma(static_cast<double>(i), limits.step, limits.minimum), limits.minimum,
                                        limits.maximum));
        }
        points.push_back(limits.maximum);
    }
    for (const auto& mark : marks) {
        points.push_back(mark.value);
    }
    if (dots && marks_only) {
        points.push_back(limits.minimum);
        points.push_back(limits.maximum);
    }
    std::ranges::sort(points);
    points.erase(std::unique(points.begin(), points.end()), points.end());
    if (points.size() > 4096) {
        throw std::invalid_argument("Slider dots exceed 4096 visual points");
    }
    return points;
}

inline double normalize_slider_value(double value, SliderLimits limits, std::span<const SliderMark> marks = {},
                                     bool marks_only = false) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Slider value must be finite");
    }
    value = std::clamp(value, limits.minimum, limits.maximum);
    const double index = std::round((value - limits.minimum) / limits.step);
    double best = marks_only ? limits.minimum
                             : std::clamp(std::fma(index, limits.step, limits.minimum), limits.minimum, limits.maximum);
    const auto consider = [&](double candidate) {
        if (std::abs(candidate - value) < std::abs(best - value) ||
            (std::abs(candidate - value) == std::abs(best - value) && candidate > best)) {
            best = candidate;
        }
    };
    consider(limits.maximum);
    const auto next = std::lower_bound(marks.begin(), marks.end(), value,
                                       [](const SliderMark& mark, double candidate) { return mark.value < candidate; });
    if (next != marks.end()) {
        consider(next->value);
    }
    if (next != marks.begin()) {
        consider(std::prev(next)->value);
    }
    return best;
}

inline SliderRange normalize_slider_range(SliderRange value, SliderLimits limits,
                                          std::span<const SliderMark> marks = {}, bool marks_only = false) {
    value = {normalize_slider_value(value.lower, limits, marks, marks_only),
             normalize_slider_value(value.upper, limits, marks, marks_only)};
    if (value.lower > value.upper) {
        std::swap(value.lower, value.upper);
    }
    return value;
}

inline void validate_slider_range_options(SliderRangeOptions options, bool marks_only, std::size_t count) {
    if (options.min_count > options.max_count || options.max_count > 64 || count < options.min_count ||
        count > options.max_count || (options.draggable_track && (options.editable || marks_only))) {
        throw std::invalid_argument("Slider range options or count are invalid");
    }
}

inline SliderValues normalize_slider_values(std::span<const double> values, SliderLimits limits,
                                            std::span<const SliderMark> marks = {}, bool marks_only = false) {
    if (values.size() > 64) {
        throw std::invalid_argument("Slider supports at most 64 handles");
    }
    SliderValues result;
    result.reserve(values.size());
    for (double value : values) {
        result.push_back(normalize_slider_value(value, limits, marks, marks_only));
    }
    std::ranges::sort(result);
    return result;
}

inline double advance_slider_value(double value, SliderLimits limits, std::span<const SliderMark> marks,
                                   bool marks_only, int direction, int steps) {
    for (int step = 0; step < steps; ++step) {
        double next = direction > 0 ? limits.maximum : limits.minimum;
        if (!marks_only) {
            const double position = (value - limits.minimum) / limits.step;
            double index = direction > 0 ? std::floor(position) + 1 : std::ceil(position) - 1;
            next = std::fma(index, limits.step, limits.minimum);
            if ((direction > 0 && next <= value) || (direction < 0 && next >= value)) {
                index += direction;
                next = std::fma(index, limits.step, limits.minimum);
            }
            next = std::clamp(next, limits.minimum, limits.maximum);
        }
        if (direction > 0) {
            const auto mark =
                std::upper_bound(marks.begin(), marks.end(), value,
                                 [](double candidate, const SliderMark& item) { return candidate < item.value; });
            if (mark != marks.end()) {
                next = std::min(next, mark->value);
            }
        } else {
            const auto mark =
                std::lower_bound(marks.begin(), marks.end(), value,
                                 [](const SliderMark& item, double candidate) { return item.value < candidate; });
            if (mark != marks.begin()) {
                next = std::max(next, std::prev(mark)->value);
            }
        }
        value = next;
    }
    return value;
}

inline bool slider_inverted(SliderOrientation orientation, bool reverse) noexcept {
    return (orientation == SliderOrientation::Vertical) != reverse;
}
} // namespace ryn::detail
