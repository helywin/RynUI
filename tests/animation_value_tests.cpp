#include "animation/value.hpp"

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

template <typename Exception, typename Operation>
void require_throws(Operation&& operation, const char* message) {
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_scalar_and_color_interpolation() {
    using namespace ryn::animation;
    const AnimationValue scalar_from = 10.0F;
    const AnimationValue scalar_to = 30.0F;
    require(interpolate_animation_value(scalar_from, scalar_to, 0.0F)
                == scalar_from,
            "scalar start endpoint was not exact");
    require(std::get<float>(
                interpolate_animation_value(scalar_from, scalar_to, 0.25F))
                == 15.0F,
            "scalar midpoint interpolation is incorrect");
    require(interpolate_animation_value(scalar_from, scalar_to, 1.0F)
                == scalar_to,
            "scalar end endpoint was not exact");
    require(std::get<float>(
                interpolate_animation_value(scalar_from, scalar_to, 1.5F))
                == 40.0F,
            "scalar overshoot was clamped by the value layer");

    const AnimationValue color_from = ryn::Color{0.25F, 0.5F, 0.75F, 1.0F};
    const AnimationValue color_to = ryn::Color{0.75F, 0.0F, 0.25F, 0.5F};
    require(std::get<ryn::Color>(
                interpolate_animation_value(color_from, color_to, 0.5F))
                == ryn::Color{0.5F, 0.25F, 0.5F, 0.75F},
            "sRGB Color interpolation is incorrect");
    require(std::get<ryn::Color>(
                interpolate_animation_value(color_from, color_to, 2.0F))
                == ryn::Color{1.0F, 0.0F, 0.0F, 0.0F},
            "Color adapter did not saturate overshoot to its valid channel range");
}

void test_geometry_interpolation() {
    using namespace ryn::animation;
    const AnimationValue point_from = ryn::runtime::Point{2.0F, 4.0F};
    const AnimationValue point_to = ryn::runtime::Point{6.0F, 12.0F};
    require(std::get<ryn::runtime::Point>(
                interpolate_animation_value(point_from, point_to, 0.5F))
                == ryn::runtime::Point{4.0F, 8.0F},
            "Point interpolation is incorrect");

    const AnimationValue size_from = ryn::runtime::Size{10.0F, 20.0F};
    const AnimationValue size_to = ryn::runtime::Size{30.0F, 60.0F};
    require(std::get<ryn::runtime::Size>(
                interpolate_animation_value(size_from, size_to, 0.25F))
                == ryn::runtime::Size{15.0F, 30.0F},
            "Size interpolation is incorrect");

    const AnimationValue rect_from = ryn::runtime::Rect{0.0F, 10.0F, 20.0F, 30.0F};
    const AnimationValue rect_to = ryn::runtime::Rect{10.0F, 30.0F, 60.0F, 90.0F};
    require(std::get<ryn::runtime::Rect>(
                interpolate_animation_value(rect_from, rect_to, 0.5F))
                == ryn::runtime::Rect{5.0F, 20.0F, 40.0F, 60.0F},
            "Rect interpolation is incorrect");

    const AnimationValue offset_from = ryn::LogicalOffset{2.0F, -2.0F};
    const AnimationValue offset_to = ryn::LogicalOffset{6.0F, 10.0F};
    require(std::get<ryn::LogicalOffset>(
                interpolate_animation_value(offset_from, offset_to, 0.5F))
                == ryn::LogicalOffset{4.0F, 4.0F},
            "LogicalOffset interpolation is incorrect");
}

void test_type_and_finite_validation() {
    using namespace ryn::animation;
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(interpolate_animation_value(
                AnimationValue{1.0F},
                AnimationValue{ryn::runtime::Point{}},
                0.5F));
        },
        "mismatched animation value kinds were accepted");
    require_throws<std::invalid_argument>(
        [] {
            validate_animation_value(AnimationValue{
                std::numeric_limits<float>::quiet_NaN()});
        },
        "NaN scalar animation value was accepted");
    require_throws<std::invalid_argument>(
        [] {
            validate_animation_value(AnimationValue{ryn::runtime::Point{
                std::numeric_limits<float>::infinity(), 0.0F}});
        },
        "infinite geometry animation value was accepted");
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(interpolate_animation_value(
                AnimationValue{0.0F},
                AnimationValue{1.0F},
                std::numeric_limits<float>::quiet_NaN()));
        },
        "NaN interpolation progress was accepted");
    require_throws<std::overflow_error>(
        [] {
            static_cast<void>(interpolate_animation_value(
                AnimationValue{std::numeric_limits<float>::max()},
                AnimationValue{-std::numeric_limits<float>::max()},
                -std::numeric_limits<float>::max()));
        },
        "overflowing scalar interpolation was accepted");
}

} // namespace

int main() {
    try {
        test_scalar_and_color_interpolation();
        test_geometry_interpolation();
        test_type_and_finite_validation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
