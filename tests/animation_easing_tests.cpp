#include "animation/easing.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_close(float actual, float expected, float tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Operation>
void require_throws(Operation&& operation, const char* message) {
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_linear_and_hybrid_solver_reference_values() {
    using ryn::animation::Easing;
    require(Easing::linear().sample(-1.0F) == 0.0F
                && Easing::linear().sample(0.25F) == 0.25F
                && Easing::linear().sample(2.0F) == 1.0F,
            "linear easing did not clamp only its time domain");

    const auto identity = Easing::cubic_bezier({0.0F, 0.0F, 1.0F, 1.0F});
    require_close(identity.sample(0.25F), 0.25F, 1.0e-5F,
                  "identity cubic-bezier reference sample drifted");

    const auto flat_derivative = Easing::cubic_bezier({0.0F, 0.0F, 0.0F, 1.0F});
    require_close(flat_derivative.sample(0.125F), 0.5F, 1.0e-5F,
                  "cubic-bezier bisection fallback reference sample drifted");

    const auto ease_in_out = ryn::animation::ant_easing(
        ryn::animation::AntEasingPreset::ease_in_out);
    require_close(ease_in_out.sample(0.5F), 0.516875F, 1.0e-5F,
                  "Ant easeInOut midpoint reference sample drifted");
}

void test_ant_design_presets_and_overshoot() {
    using namespace ryn::animation;
    struct Expected final {
        AntEasingPreset preset;
        ryn::CubicBezier curve;
    };
    const std::array expected{
        Expected{AntEasingPreset::ease_out_circ, {0.08F, 0.82F, 0.17F, 1.0F}},
        Expected{AntEasingPreset::ease_in_out_circ, {0.78F, 0.14F, 0.15F, 0.86F}},
        Expected{AntEasingPreset::ease_out, {0.215F, 0.61F, 0.355F, 1.0F}},
        Expected{AntEasingPreset::ease_in_out, {0.645F, 0.045F, 0.355F, 1.0F}},
        Expected{AntEasingPreset::ease_out_back, {0.12F, 0.4F, 0.29F, 1.46F}},
        Expected{AntEasingPreset::ease_in_back, {0.71F, -0.46F, 0.88F, 0.6F}},
        Expected{AntEasingPreset::ease_in_quint, {0.755F, 0.05F, 0.855F, 0.06F}},
        Expected{AntEasingPreset::ease_out_quint, {0.23F, 1.0F, 0.32F, 1.0F}},
    };
    for (const auto& item : expected) {
        const auto easing = ant_easing(item.preset);
        require(easing.kind() == EasingKind::cubic_bezier
                    && easing.curve() == item.curve,
                "Ant Design easing preset control points drifted");
        require(easing.sample(0.0F) == 0.0F && easing.sample(1.0F) == 1.0F,
                "Ant Design easing endpoints were not exact");
    }
    require(ant_easing(AntEasingPreset::ease_out_back).sample(0.8F) > 1.0F,
            "easeOutBack overshoot was incorrectly clamped");
    require(ant_easing(AntEasingPreset::ease_in_back).sample(0.2F) < 0.0F,
            "easeInBack undershoot was incorrectly clamped");
}

void test_invalid_inputs() {
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(ryn::animation::Easing::linear().sample(
                std::numeric_limits<float>::quiet_NaN()));
        },
        "easing accepted NaN input");
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(ryn::CubicBezier{-0.1F, 0.0F, 1.0F, 1.0F});
        },
        "cubic-bezier accepted an invalid x control point");
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(ryn::CubicBezier{
                0.0F,
                std::numeric_limits<float>::infinity(),
                1.0F,
                1.0F});
        },
        "cubic-bezier accepted an infinite y control point");
}

} // namespace

int main() {
    try {
        test_linear_and_hybrid_solver_reference_values();
        test_ant_design_presets_and_overshoot();
        test_invalid_inputs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
