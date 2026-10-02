#include "renderer/common/scene_packing.hpp"
#include "renderer/common/rounded_effect_packing.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace ryn;
using namespace ryn::detail;
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <std::size_t N>
void near(const std::array<float, N> &actual, const std::array<float, N> &expected) {
    for (std::size_t i = 0; i < N; ++i)
        check(std::fabs(actual[i] - expected[i]) < 0.00001F, "incorrect packed coordinate");
}
template <class F> void rejects(F action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "invalid packing input accepted");
}
void literal_packing() {
    static_assert(!std::is_same_v<graphics::QuadInstance, QuadGpuInstance>);
    static_assert(!std::is_same_v<graphics::GlyphInstance, GlyphGpuInstance>);
    static_assert(graphics::logical_scene_version == 2 && packed_scene_abi_version == 1);
    const graphics::QuadInstance quad{
        {10, 20, 30, 40}, {0.2F, 0.4F, 0.6F, 0.8F}, 0.75F, 6, {5, -10}};
    const auto gpu = pack_quad_instance(quad, {400, 200, 2}); // logical viewport 200 x 100
    const auto viewport = scene_logical_viewport({400, 200, 2});
    check(viewport == runtime::Rect{0, 0, 200, 100}, "common logical viewport changed");
    const auto effect = graphics::make_outline_effect({{10, 20, 30, 40}, 6}, 3, 1,
        Color::rgba8(22, 119, 255));
    const auto effect_gpu = pack_rounded_effect_instance(effect, {400, 200, 2});
    check(effect_gpu.shape_rect == std::array<float, 4>{20, 40, 60, 80},
        "Effect did not consume the common metrics scale");
    near(gpu.clip_rect, {-0.9F, 0.6F, 0.3F, -0.8F});
    near(gpu.translation, {0.05F, 0.2F});
    check(gpu.corner_radius == 0.2F && gpu.color == quad.color && gpu.opacity == quad.opacity,
          "Quad packing lost material or normalized radius");
    auto oversized = quad;
    oversized.corner_radius = 200;
    check(pack_quad_instance(oversized, {400, 200, 2}).corner_radius == 0.5F,
          "Quad radius was not clamped at packing boundary");
    oversized.bounds[2] = 0;
    check(pack_quad_instance(oversized, {400, 200, 2}).corner_radius == 0,
          "zero-area Quad radius was not finite zero");
    const graphics::GlyphInstance glyph{{12.5F, 24, 8, 10},
                                        {0, 0.25F, 0.5F, 1},
                                        {10, 20, 110, 80},
                                        {0.2F, 0.4F, 0.6F, 0.8F},
                                        {5, -10, 0.5F, 0}};
    const auto glyph_gpu = pack_glyph_instance(glyph, {400, 200, 2});
    near(glyph_gpu.position_size, {-0.875F, 0.52F, 0.08F, -0.2F});
    near(glyph_gpu.clip_bounds, {-0.9F, 0.6F, 0.1F, -0.6F});
    near(glyph_gpu.translation_opacity, {0.05F, 0.2F, 0.5F, 0});
    check(glyph_gpu.uv_rect == glyph.uv_rect && glyph_gpu.color == glyph.color,
          "Glyph packing changed normalized UV or color");
    near(pack_quad_instance(quad, {800, 400, 2}).clip_rect, {-0.95F, 0.8F, 0.15F, -0.4F});
    check(quad.bounds == std::array<float, 4>{10, 20, 30, 40}, "packing changed CPU bounds");
    for (auto metrics : {SceneDeviceMetrics{0, 200, 1}, SceneDeviceMetrics{400, 0, 1},
                         SceneDeviceMetrics{400, 200, 0}, SceneDeviceMetrics{400, 200, -1},
                         SceneDeviceMetrics{400, 200, std::numeric_limits<float>::quiet_NaN()},
                         SceneDeviceMetrics{400, 200, std::numeric_limits<float>::infinity()},
                         SceneDeviceMetrics{400, 200, std::numeric_limits<float>::denorm_min()}}) {
        rejects([&] { validate_scene_device_metrics(metrics); });
        rejects([&] { (void)pack_quad_instance(quad, metrics); });
        rejects([&] { (void)pack_glyph_instance(glyph, metrics); });
        rejects([&] { (void)pack_rounded_effect_instance(effect, metrics); });
    }
    auto invalid = quad;
    invalid.bounds[3] = -1;
    rejects([&] { (void)pack_quad_instance(invalid, {400, 200, 2}); });
}
} // namespace
int main() {
    try {
        literal_packing();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
