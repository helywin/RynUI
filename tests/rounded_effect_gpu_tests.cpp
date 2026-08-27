#include "graphics/rounded_effect_gpu.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

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

ryn::graphics::RoundedEffectInstance outer() {
    return ryn::graphics::make_shadow_effect(
        {{10.0F, 20.0F, 100.0F, 40.0F}, 8.0F},
        {ryn::ShadowKind::outer, {4.0F, -2.0F}, 8.0F, 3.0F,
         ryn::Color::rgba8(10, 20, 30, 64)});
}

void test_instance_layout_and_device_packing() {
    static_assert(sizeof(ryn::graphics::RoundedEffectGpuInstance) == 112);
    const auto packed = ryn::graphics::pack_rounded_effect_instance(
        outer(), {200, 100, 1.0F});
    require(near(packed.clip_rect[0], -1.0F)
                && near(packed.clip_rect[1], 0.96F)
                && near(packed.clip_rect[2], 1.3F)
                && near(packed.clip_rect[3], -1.44F),
            "rounded-effect bounds did not convert to the expected NDC quad");
    require(packed.shape_rect == std::array{10.0F, 20.0F, 100.0F, 40.0F}
                && packed.clip_bounds == std::array{0.0F, 0.0F, 200.0F, 100.0F}
                && packed.color
                    == std::array{10.0F / 255.0F, 20.0F / 255.0F,
                                  30.0F / 255.0F, 64.0F / 255.0F}
                && packed.shadow_params == std::array{8.0F, 4.0F, -2.0F, 8.0F}
                && packed.effect_params == std::array{3.0F, 0.0F, 0.0F, 0.0F}
                && packed.material_params == std::array{1.0F, 1.0F, 0.0F, 0.0F},
            "rounded-effect GPU instance field layout or value packing changed");
}

void test_100_150_200_percent_scale_contract() {
    const auto source = outer();
    const auto at_100 = ryn::graphics::pack_rounded_effect_instance(
        source, {200, 100, 1.0F});
    const auto at_150 = ryn::graphics::pack_rounded_effect_instance(
        source, {300, 150, 1.5F});
    const auto at_200 = ryn::graphics::pack_rounded_effect_instance(
        source, {400, 200, 2.0F});
    for (std::size_t index = 0; index < 4; ++index) {
        require(near(at_100.clip_rect[index], at_150.clip_rect[index], 0.021F)
                    && near(at_100.clip_rect[index], at_200.clip_rect[index], 0.021F),
                "effect NDC envelope drift exceeds its fixed one-pixel AA guard");
    }
    for (std::size_t index = 0; index < 4; ++index) {
        require(near(at_150.shape_rect[index], at_100.shape_rect[index] * 1.5F)
                    && near(at_200.shape_rect[index], at_100.shape_rect[index] * 2.0F),
                "shape geometry did not scale uniformly to device pixels");
    }
    require(near(at_150.shadow_params[0], 12.0F)
                && near(at_150.shadow_params[1], 6.0F)
                && near(at_150.shadow_params[2], -3.0F)
                && near(at_150.shadow_params[3], 12.0F)
                && near(at_200.effect_params[0], 6.0F)
                && at_150.material_params[1] == 1.0F
                && at_200.material_params[1] == 1.0F,
            "effect parameters or physical AA width scaled incorrectly");
}

void test_shader_reference_matches_logical_reference() {
    const auto shadow = outer();
    for (const float scale : {1.0F, 1.5F, 2.0F}) {
        const auto packed = ryn::graphics::pack_rounded_effect_instance(
            shadow,
            {static_cast<std::uint32_t>(200.0F * scale),
             static_cast<std::uint32_t>(100.0F * scale),
             scale});
        for (const auto logical_point : std::array{
                 ryn::runtime::Point{20.0F, 30.0F},
                 ryn::runtime::Point{114.0F, 40.0F},
                 ryn::runtime::Point{124.0F, 40.0F}}) {
            const float logical = ryn::graphics::rounded_effect_coverage(
                logical_point, shadow, 1.0F / scale);
            const float gpu = ryn::graphics::rounded_effect_gpu_coverage_reference(
                {logical_point.x * scale, logical_point.y * scale}, packed);
            require(near(logical, gpu, 0.00001F),
                    "GPU shadow reference drifted from logical SDF coverage");
        }
    }

    auto outline = ryn::graphics::make_outline_effect(
        {{20.0F, 20.0F, 80.0F, 32.0F}, 6.0F},
        3.0F,
        1.0F,
        ryn::Color::rgba8(22, 119, 255),
        0.8F,
        {},
        ryn::graphics::EffectClip{5, {0.0F, 0.0F, 110.0F, 80.0F}});
    const auto packed_outline = ryn::graphics::pack_rounded_effect_instance(
        outline, {200, 100, 1.0F});
    require(near(ryn::graphics::rounded_effect_gpu_coverage_reference(
                     {100.5F, 36.0F}, packed_outline), 0.0F)
                && ryn::graphics::rounded_effect_gpu_coverage_reference(
                       {102.5F, 36.0F}, packed_outline) > 0.99F
                && near(ryn::graphics::rounded_effect_gpu_coverage_reference(
                            {102.5F, 90.0F}, packed_outline), 0.0F),
            "GPU outline gap, ring, or ancestor clip reference is incorrect");
}

std::array<float, 4> source_over(
    std::array<float, 4> source,
    std::array<float, 4> destination) {
    return {
        source[0] * source[3] + destination[0] * (1.0F - source[3]),
        source[1] * source[3] + destination[1] * (1.0F - source[3]),
        source[2] * source[3] + destination[2] * (1.0F - source[3]),
        source[3] + destination[3] * (1.0F - source[3]),
    };
}

void test_straight_alpha_and_overlapping_layer_contract() {
    auto shadow = outer();
    shadow.material.color = ryn::Color(0.2F, 0.4F, 0.8F, 0.5F);
    shadow.material.opacity = 0.5F;
    const auto packed = ryn::graphics::pack_rounded_effect_instance(
        shadow, {200, 100, 1.0F});
    const auto fragment = ryn::graphics::rounded_effect_gpu_fragment_reference(
        {30.0F, 30.0F}, packed);
    const float coverage = ryn::graphics::rounded_effect_gpu_coverage_reference(
        {30.0F, 30.0F}, packed);
    require(near(fragment[0], 0.2F) && near(fragment[1], 0.4F)
                && near(fragment[2], 0.8F)
                && near(fragment[3], 0.25F * coverage),
            "effect fragment reference premultiplied RGB or lost alpha coverage");

    const auto transparent = source_over(fragment, {0.0F, 0.0F, 0.0F, 0.0F});
    require(near(transparent[0], fragment[0] * fragment[3])
                && near(transparent[3], fragment[3]),
            "straight-alpha effect blend over transparency is inconsistent");

    const std::array<float, 4> black_layer{0.0F, 0.0F, 0.0F, 0.25F};
    const std::array<float, 4> white{1.0F, 1.0F, 1.0F, 1.0F};
    const auto once = source_over(black_layer, white);
    const auto twice = source_over(black_layer, once);
    require(near(once[0], 0.75F) && near(twice[0], 0.5625F)
                && near(twice[3], 1.0F),
            "overlapping shadow layers used additive or double-premultiplied alpha");
}

void test_invalid_metrics_and_clipped_pack_rejected() {
    require_invalid([] {
        ryn::graphics::validate_rounded_effect_device_metrics({0, 100, 1.0F});
    }, "zero-width effect metrics were accepted");
    require_invalid([] {
        ryn::graphics::validate_rounded_effect_device_metrics(
            {100, 100, std::numeric_limits<float>::quiet_NaN()});
    }, "NaN effect display scale was accepted");
    require_invalid([] {
        auto clipped = outer();
        clipped.geometry.ancestor_clip = ryn::graphics::EffectClip{
            8, {500.0F, 500.0F, 20.0F, 20.0F}};
        static_cast<void>(ryn::graphics::pack_rounded_effect_instance(
            clipped, {200, 100, 1.0F}));
    }, "fully clipped rounded effect was packed for GPU upload");
}

} // namespace

int main() {
    try {
        test_instance_layout_and_device_packing();
        test_100_150_200_percent_scale_contract();
        test_shader_reference_matches_logical_reference();
        test_straight_alpha_and_overlapping_layer_contract();
        test_invalid_metrics_and_clipped_pack_rejected();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
