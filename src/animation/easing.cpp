#include "animation/easing.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::animation {
namespace {

double curve_coordinate(double first, double second, double parameter) noexcept {
    const double inverse = 1.0 - parameter;
    return 3.0 * first * inverse * inverse * parameter
        + 3.0 * second * inverse * parameter * parameter
        + parameter * parameter * parameter;
}

double curve_derivative(double first, double second, double parameter) noexcept {
    const double inverse = 1.0 - parameter;
    return 3.0 * first * inverse * inverse
        + 6.0 * (second - first) * inverse * parameter
        + 3.0 * (1.0 - second) * parameter * parameter;
}

double solve_curve_parameter(CubicBezier curve, double progress) noexcept {
    constexpr double derivative_epsilon = 1.0e-7;
    constexpr double solution_epsilon = 1.0e-8;
    constexpr int maximum_iterations = 20;

    double lower = 0.0;
    double upper = 1.0;
    double parameter = progress;
    for (int iteration = 0; iteration < maximum_iterations; ++iteration) {
        const double coordinate = curve_coordinate(curve.x1, curve.x2, parameter);
        const double error = coordinate - progress;
        if (std::abs(error) <= solution_epsilon) {
            return parameter;
        }
        if (error < 0.0) {
            lower = parameter;
        } else {
            upper = parameter;
        }

        const double derivative = curve_derivative(curve.x1, curve.x2, parameter);
        const double newton = derivative > derivative_epsilon
            ? parameter - error / derivative
            : -1.0;
        parameter = newton > lower && newton < upper
            ? newton
            : (lower + upper) * 0.5;
    }
    return parameter;
}

} // namespace

float Easing::sample(float normalized_time) const {
    if (!std::isfinite(normalized_time)) {
        throw std::invalid_argument("easing input must be finite");
    }
    const double progress = std::clamp(
        static_cast<double>(normalized_time), 0.0, 1.0);
    if (progress == 0.0 || progress == 1.0 || kind_ == EasingKind::linear) {
        return static_cast<float>(progress);
    }
    const double parameter = solve_curve_parameter(curve_, progress);
    return static_cast<float>(
        curve_coordinate(curve_.y1, curve_.y2, parameter));
}

Easing ant_easing(AntEasingPreset preset) {
    switch (preset) {
    case AntEasingPreset::ease_out_circ:
        return Easing::cubic_bezier({0.08F, 0.82F, 0.17F, 1.0F});
    case AntEasingPreset::ease_in_out_circ:
        return Easing::cubic_bezier({0.78F, 0.14F, 0.15F, 0.86F});
    case AntEasingPreset::ease_out:
        return Easing::cubic_bezier({0.215F, 0.61F, 0.355F, 1.0F});
    case AntEasingPreset::ease_in_out:
        return Easing::cubic_bezier({0.645F, 0.045F, 0.355F, 1.0F});
    case AntEasingPreset::ease_out_back:
        return Easing::cubic_bezier({0.12F, 0.4F, 0.29F, 1.46F});
    case AntEasingPreset::ease_in_back:
        return Easing::cubic_bezier({0.71F, -0.46F, 0.88F, 0.6F});
    case AntEasingPreset::ease_in_quint:
        return Easing::cubic_bezier({0.755F, 0.05F, 0.855F, 0.06F});
    case AntEasingPreset::ease_out_quint:
        return Easing::cubic_bezier({0.23F, 1.0F, 0.32F, 1.0F});
    }
    throw std::invalid_argument("Ant Design easing preset is invalid");
}

} // namespace ryn::animation
