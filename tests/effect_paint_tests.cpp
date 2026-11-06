#include "graphics/effect_paint.hpp"
#include "graphics/rounded_effect.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
using namespace ryn;
using namespace ryn::graphics;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.00001F;
}

template <class F> void rejects(F&& action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid paint value was accepted");
}

EffectGradientRamp ramp() {
    return {{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1, 1}}};
}

EffectPaintField constant(Color color) {
    return {EffectGradientRamp{{0, color}, {1, color}}, EffectLinearDomain{{0, 0}, {10, 0}}};
}

void ownership_order_and_sampling() {
    std::vector<EffectGradientStop> source{
        {-1, Color{1, 0, 0}}, {0.5F, Color{0, 1, 0}}, {0.5F, Color{0, 0, 1}}, {2, Color{1, 1, 1}}};
    const EffectGradientRamp copied{source};
    source[2].color = Color{};
    source.clear();
    source.shrink_to_fit();
    require(copied.stops().size() == 4 && effect_ramp_reference(copied, 0.5F) == Color{0, 0, 1},
            "ramp borrowed source storage or duplicate stops were not right-continuous");
    require(effect_ramp_reference(copied, -100) == Color{1, 0, 0} &&
                effect_ramp_reference(copied, 100) == Color{1, 1, 1},
            "out-of-domain stops were clamped, dropped, or extrapolated");
    require(effect_ramp_reference(copied, 0.5F - 0.0001F).green() > 0.999F,
            "duplicate stop changed its left-hand interpolation");
    const EffectGradientRamp coincident{{0, Color{1, 0, 0}}, {0, Color{0, 1, 0}}, {0, Color{0, 0, 1}}};
    require(effect_ramp_reference(coincident, -1) == Color{1, 0, 0} &&
                effect_ramp_reference(coincident, 0) == Color{0, 0, 1},
            "all-coincident ramp lost endpoint holding or right continuity");
    std::vector<EffectGradientStop> many;
    for (int index = 0; index < 257; ++index) {
        const auto position = float(index) / 256;
        many.push_back({position, Color{position, 0, 1 - position}});
    }
    const EffectGradientRamp large{many};
    require(large.stops().size() == 257 && near(effect_ramp_reference(large, 0.5F).red(), 0.5F),
            "common ramp imposed a small stop limit");
    const auto interpolated = effect_ramp_reference(ramp(), 0.5F);
    require(interpolated.red() == 0 && interpolated.blue() == 1 && interpolated.alpha() == 0.5F,
            "transparent red to blue used straight RGB interpolation");
}

void domains_composition_and_fragment() {
    const EffectPaintField linear{ramp(), EffectLinearDomain{{2, 3}, {12, 3}}};
    require(effect_field_reference(linear, {7, 200}) == Color{0, 0, 1, 0.5F},
            "linear projection followed sample y or lost the declared domain");
    constexpr auto pi = std::numbers::pi_v<float>;
    const EffectPaintField conic{ramp(), EffectConicDomain{{10, 20}, 0, 2 * pi}};
    require(near(effect_field_reference(conic, {9, 20}).alpha(), 0.5F) &&
                near(effect_field_reference(conic, {10, 21}).alpha(), 0.25F) &&
                near(effect_field_reference(conic, {10, 19}).alpha(), 0.75F),
            "conic domain lost its clockwise full-circle coordinates");
    require(conic == EffectPaintField{ramp(), EffectConicDomain{{10, 20}, 2 * pi, 2 * pi}},
            "equivalent conic rotations have different normalized values");
    const EffectPaint paint{constant(Color{1, 0, 0, 0.5F}), constant(Color{0, 0, 1, 0.5F})};
    const auto sample = effect_paint_reference(paint, {5, 0}, 1, 0.5F);
    require(near(sample.red(), 2.0F / 3) && near(sample.blue(), 1.0F / 3) && sample.alpha() == 0.375F,
            "dual fields applied opacity per field or composited in the wrong order");
    require(effect_paint_reference(paint, {5, 0}, 0.5F, 0.5F).alpha() == 0.1875F,
            "dual fields did not apply coverage once after composition");
    const EffectPaint mixed{conic, constant(Color{1, 0, 0, 0.5F})};
    const auto composite = effect_paint_reference(mixed, {9, 20}, 1, 0.5F);
    require(near(composite.red(), 1.0F / 3) && near(composite.blue(), 2.0F / 3) && composite.alpha() == 0.375F,
            "conic foreground over linear underlay lost the single-fragment alpha contract");
    auto effect = make_shadow_effect({{0, 0, 10, 10}, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 1, 1}});
    effect.material.paint = EffectPaint{linear, std::nullopt};
    effect.geometry.translation = {100, 50};
    effect.material.opacity = 0.5F;
    const auto fragment = rounded_effect_fragment_reference({107, 55}, effect, 1, 0.5F);
    require(fragment.blue() == 1 && fragment.red() == 0 && near(fragment.alpha(), 0.125F),
            "logical fragment lost retained field translation or multiplied ancestor opacity twice");
    effect.geometry.ancestor_clip = EffectClip{1, {100, 50, 7, 10}};
    require(rounded_effect_fragment_reference({107, 55}, effect).alpha() == 0,
            "logical paint fragment lost the half-open ancestor clip");
}

void value_equality_and_store_dirties() {
    RoundedEffectStore store;
    auto effect = make_arc_effect({30, 30}, 10, 2, 0, 90, Color{1, 0, 0});
    effect.material.paint = EffectPaint{{ramp(), EffectLinearDomain{{20, 20}, {40, 20}}}, std::nullopt};
    const auto id = store.add(effect);
    require(store.compact({0, 0, 100, 100}), "paint store fixture did not compact");
    store.clear_dirty_ranges();
    auto material = effect.material;
    material.paint = EffectPaint{{ramp(), EffectLinearDomain{{20, 20}, {40, 20}}}, std::nullopt};
    require(!store.update_material(id, material) && store.material_dirty_ranges().empty(),
            "equal separately allocated paint dirtied the store");
    material.paint = EffectPaint{{ramp(), EffectLinearDomain{{20, 20}, {50, 20}}}, std::nullopt};
    require(store.update_material(id, material) && store.material_dirty_ranges().size() == 1 &&
                !store.compact({0, 0, 100, 100}) && store.packed_instances().front().material == material,
            "paint update did not patch only the material range");
    store.clear_dirty_ranges();
    auto geometry = effect.geometry;
    std::get<RoundedArcPath>(geometry.path).cap = ArcCap::Square;
    require(store.update_geometry(id, geometry) && store.geometry_dirty_ranges().size() == 1 &&
                !store.compact({0, 0, 100, 100}) && store.material_dirty_ranges().empty(),
            "cap update did not use the existing geometry dirty path");
    require(!store.update_geometry(id, geometry), "equal cap geometry requested new work");
    auto invalid = geometry;
    std::get<RoundedArcPath>(invalid.path).cap = static_cast<ArcCap>(255);
    rejects([&] { static_cast<void>(store.update_geometry(id, invalid)); });
    require(store.at(id).geometry == geometry && store.geometry_dirty_ranges().size() == 1,
            "invalid cap update partially published geometry or dirty metadata");
}

void invalid_values() {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    rejects([] { static_cast<void>(EffectGradientRamp{std::span<const EffectGradientStop>{}}); });
    rejects([] { static_cast<void>(EffectGradientRamp{{0, Color{}}}); });
    rejects([] { static_cast<void>(EffectGradientRamp{{1, Color{}}, {0, Color{}}}); });
    for (const auto invalid : {nan, inf, -inf}) {
        rejects([&] { static_cast<void>(EffectGradientRamp{{0, Color{}}, {invalid, Color{}}}); });
        rejects([&] { static_cast<void>(EffectPaintField{ramp(), EffectLinearDomain{{0, 0}, {invalid, 0}}}); });
        rejects([&] { static_cast<void>(EffectPaintField{ramp(), EffectConicDomain{{invalid, 0}, 0, 1}}); });
        rejects([&] { static_cast<void>(EffectPaintField{ramp(), EffectConicDomain{{0, 0}, invalid, 1}}); });
        rejects([&] { static_cast<void>(EffectPaintField{ramp(), EffectConicDomain{{0, 0}, 0, invalid}}); });
    }
    rejects([] { static_cast<void>(EffectPaintField{ramp(), EffectLinearDomain{{1, 2}, {1, 2}}}); });
    for (const auto invalid : {0.0F, -1.0F, 7.0F}) {
        rejects([&] { static_cast<void>(EffectPaintField{ramp(), EffectConicDomain{{0, 0}, 0, invalid}}); });
    }
    rejects([&] { static_cast<void>(effect_ramp_reference(ramp(), nan)); });
    const EffectPaint paint{constant(Color{1, 1, 1}), std::nullopt};
    rejects([&] { static_cast<void>(effect_paint_reference(paint, {0, 0}, nan)); });
    rejects([&] { static_cast<void>(effect_paint_reference(paint, {0, 0}, 1, 2)); });
}
} // namespace

int main() {
    try {
        ownership_order_and_sampling();
        domains_composition_and_fragment();
        value_equality_and_store_dirties();
        invalid_values();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
