#include "graphics/rounded_effect.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using ryn::graphics::LogicalRoundedRect;
using ryn::graphics::RoundedEffectInstance;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right, float tolerance = 0.0001F) {
    return std::abs(left - right) <= tolerance;
}

template<typename Function>
void require_invalid(Function&& function, const char* message) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

RoundedEffectInstance outer(
    LogicalRoundedRect shape,
    float blur = 0.0F,
    float spread = 0.0F) {
    return ryn::graphics::make_shadow_effect(
        shape,
        {ryn::ShadowKind::outer, {}, blur, spread, ryn::Color::rgba8(0, 0, 0, 96)});
}

void test_instance_value_contract() {
    const LogicalRoundedRect shape{{2.0F, 3.0F, 40.0F, 20.0F}, 5.0F};
    const auto outer_effect = ryn::graphics::make_shadow_effect(
        shape,
        {ryn::ShadowKind::outer, {4.0F, -2.0F}, 8.0F, -1.0F,
         ryn::Color::rgba8(1, 2, 3, 64)},
        {7.0F, 9.0F},
        ryn::graphics::EffectClip{42, {0.0F, 0.0F, 100.0F, 80.0F}});
    require(outer_effect.geometry.shape == shape
                && outer_effect.geometry.kind
                    == ryn::graphics::RoundedEffectKind::outer_shadow
                && outer_effect.geometry.offset == ryn::LogicalOffset{4.0F, -2.0F}
                && outer_effect.geometry.blur == 8.0F
                && outer_effect.geometry.spread == -1.0F
                && outer_effect.geometry.translation == ryn::runtime::Point{7.0F, 9.0F}
                && outer_effect.geometry.ancestor_clip->identity == 42
                && outer_effect.material.color == ryn::Color::rgba8(1, 2, 3, 64)
                && outer_effect.material.opacity == 1.0F,
            "outer rounded-effect value contract lost a declared field");

    const auto inset_effect = ryn::graphics::make_shadow_effect(
        shape,
        {ryn::ShadowKind::inset, {}, 3.0F, 1.0F, ryn::Color::rgba8(4, 5, 6, 70)});
    require(inset_effect.geometry.kind
                == ryn::graphics::RoundedEffectKind::inset_shadow,
            "inset ShadowKind did not map to an inset rounded effect");
}

void test_rounded_rect_signed_distance_and_golden_mask() {
    const LogicalRoundedRect shape{{0.0F, 0.0F, 10.0F, 10.0F}, 2.0F};
    require(near(ryn::graphics::rounded_rect_signed_distance({5.0F, 5.0F}, shape), -5.0F)
                && near(ryn::graphics::rounded_rect_signed_distance({10.0F, 5.0F}, shape), 0.0F),
            "rounded-rect SDF center or edge is incorrect");
    require(near(
                ryn::graphics::rounded_rect_signed_distance({10.0F, 10.0F}, shape),
                std::sqrt(8.0F) - 2.0F),
            "rounded-rect SDF corner is incorrect");

    for (const ryn::runtime::Point point : std::array{
             ryn::runtime::Point{1.25F, 2.75F},
             ryn::runtime::Point{4.0F, 0.5F},
             ryn::runtime::Point{8.5F, 6.0F}}) {
        const float reference = ryn::graphics::rounded_rect_signed_distance(point, shape);
        require(near(reference, ryn::graphics::rounded_rect_signed_distance(
                                    {10.0F - point.x, point.y}, shape))
                    && near(reference, ryn::graphics::rounded_rect_signed_distance(
                                           {point.x, 10.0F - point.y}, shape)),
                "rounded-rect SDF lost horizontal or vertical symmetry");
    }

    constexpr std::array<int, 25> expected_mask{
        0, 1, 1, 1, 0,
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 1,
        0, 1, 1, 1, 0,
    };
    const LogicalRoundedRect golden{{0.0F, 0.0F, 5.0F, 5.0F}, 2.5F};
    for (std::size_t index = 0; index < expected_mask.size(); ++index) {
        const ryn::runtime::Point sample{
            static_cast<float>(index % 5) + 0.5F,
            static_cast<float>(index / 5) + 0.5F,
        };
        const int covered =
            ryn::graphics::rounded_rect_signed_distance(sample, golden) <= 0.0F ? 1 : 0;
        require(covered == expected_mask[index], "rounded-rect embedded golden mask changed");
    }
}

void test_shadow_and_outline_coverage() {
    const LogicalRoundedRect shape{{0.0F, 0.0F, 20.0F, 20.0F}, 4.0F};
    const auto blurred = outer(shape, 8.0F, 2.0F);
    const float inside = ryn::graphics::rounded_effect_coverage({10.0F, 10.0F}, blurred);
    const float edge = ryn::graphics::rounded_effect_coverage({22.0F, 10.0F}, blurred);
    const float far = ryn::graphics::rounded_effect_coverage({40.0F, 10.0F}, blurred);
    require(inside > edge && edge > far && edge > 0.0F && edge < 1.0F,
            "outer shadow did not produce a soft Gaussian edge");

    const auto inset = ryn::graphics::make_shadow_effect(
        shape,
        {ryn::ShadowKind::inset, {2.0F, 0.0F}, 4.0F, 0.0F,
         ryn::Color::rgba8(0, 0, 0, 80)});
    require(ryn::graphics::rounded_effect_coverage({1.0F, 10.0F}, inset)
                    > ryn::graphics::rounded_effect_coverage({10.0F, 10.0F}, inset)
                && near(ryn::graphics::rounded_effect_coverage({-1.0F, 10.0F}, inset), 0.0F),
            "inset shadow direction or shape clipping is incorrect");

    const auto outline = ryn::graphics::make_outline_effect(
        shape, 3.0F, 2.0F, ryn::Color::rgba8(22, 119, 255));
    require(near(ryn::graphics::rounded_effect_coverage({20.5F, 10.0F}, outline, 0.25F), 0.0F)
                && ryn::graphics::rounded_effect_coverage({23.5F, 10.0F}, outline, 0.25F)
                    > 0.99F
                && near(ryn::graphics::rounded_effect_coverage({26.0F, 10.0F}, outline, 0.25F), 0.0F),
            "outline did not preserve its transparent gap and hollow ring");
}

void test_bounds_and_validation() {
    const LogicalRoundedRect shape{{10.0F, 20.0F, 100.0F, 40.0F}, 8.0F};
    auto effect = ryn::graphics::make_shadow_effect(
        shape,
        {ryn::ShadowKind::outer, {4.0F, -2.0F}, 8.0F, 3.0F,
         ryn::Color::rgba8(0, 0, 0, 64)});
    require(ryn::graphics::rounded_effect_bounds(effect)
                == ryn::runtime::Rect{-2.0F, 2.0F, 132.0F, 72.0F},
            "outer effect bounds omitted offset, spread, 3-sigma, or AA guard");

    effect.geometry.spread = -3.0F;
    effect.geometry.translation = {5.0F, 7.0F};
    require(ryn::graphics::rounded_effect_bounds(effect)
                == ryn::runtime::Rect{9.0F, 15.0F, 120.0F, 60.0F},
            "negative spread or retained translation produced loose bounds");

    effect.geometry.spread = -100.0F;
    require(ryn::graphics::rounded_effect_bounds(effect).width == 0.0F,
            "collapsed negative spread produced a phantom blur bound");
    effect.geometry.spread = -3.0F;

    effect.geometry.ancestor_clip = ryn::graphics::EffectClip{
        77, {20.0F, 20.0F, 30.0F, 20.0F}};
    require(ryn::graphics::rounded_effect_bounds(effect)
                == ryn::runtime::Rect{20.0F, 20.0F, 30.0F, 20.0F},
            "ancestor clip did not tighten rounded-effect bounds");
    effect.geometry.ancestor_clip = ryn::graphics::EffectClip{
        78, {500.0F, 500.0F, 10.0F, 10.0F}};
    require(ryn::graphics::rounded_effect_bounds(effect).width == 0.0F,
            "fully clipped effect retained a non-empty bound");

    const auto logical = outer(shape, 6.0F, 2.0F);
    auto physical = outer({{20.0F, 40.0F, 200.0F, 80.0F}, 16.0F}, 12.0F, 4.0F);
    physical.geometry.offset = {logical.geometry.offset.x * 2.0F,
                                logical.geometry.offset.y * 2.0F};
    const auto logical_bounds = ryn::graphics::rounded_effect_bounds(logical);
    const auto physical_bounds = ryn::graphics::rounded_effect_bounds(physical, 2.0F);
    require(near(physical_bounds.x, logical_bounds.x * 2.0F)
                && near(physical_bounds.width, logical_bounds.width * 2.0F),
            "rounded-effect bounds are not DPI-scale invariant");

    require_invalid([&] {
        static_cast<void>(ryn::graphics::make_outline_effect(
            shape, 0.0F, 1.0F, ryn::Color::rgba8(0, 0, 0)));
    }, "zero-width outline was accepted");
    require_invalid([&] {
        auto invalid = outer(shape);
        invalid.geometry.blur = std::numeric_limits<float>::quiet_NaN();
        ryn::graphics::validate_rounded_effect(invalid);
    }, "NaN rounded-effect geometry was accepted");
    require_invalid([&] {
        auto invalid = outer(shape);
        invalid.material.opacity = 1.1F;
        ryn::graphics::validate_rounded_effect(invalid);
    }, "out-of-range rounded-effect opacity was accepted");
    require_invalid([&] {
        auto invalid = outer(shape);
        invalid.geometry.kind = static_cast<ryn::graphics::RoundedEffectKind>(255);
        ryn::graphics::validate_rounded_effect(invalid);
    }, "unknown rounded-effect kind was accepted");
}

} // namespace

int main() {
    try {
        test_instance_value_contract();
        test_rounded_rect_signed_distance_and_golden_mask();
        test_shadow_and_outline_coverage();
        test_bounds_and_validation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
