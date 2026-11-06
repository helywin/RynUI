#include "graphics/rounded_effect.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::graphics;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right, float tolerance = 0.0001F) {
    return std::abs(left - right) < tolerance;
}

float distance(runtime::Point point, const RoundedEffectInstance& effect) {
    return arc_signed_distance(point, effect.geometry.shape, effect.geometry.outline_width,
                               std::get<RoundedArcPath>(effect.geometry.path));
}

void legacy_and_explicit_caps() {
    const Color color{1, 0, 0, 0.5F};
    const auto legacy = make_arc_effect({0, 0}, 10, 4, -270, 90, color);
    const auto round = make_arc_effect({0, 0}, 10, 4, -270, 90, ArcCap::Round, color);
    require(legacy == round && std::get<RoundedArcPath>(legacy.geometry.path).cap == ArcCap::Round &&
                std::get<RoundedArcPath>(legacy.geometry.path).start_radians == std::numbers::pi_v<float> / 2,
            "explicit entry changed old centerline angles or the default cap");
    const RoundedArcPath prefix{0, 1};
    const RoundedEffectMaterial material_prefix{color, 0.5F, false};
    require(prefix.cap == ArcCap::Round && !material_prefix.paint, "old aggregate prefixes lost their defaults");
    const auto disk = make_arc_effect({0, 0}, 0, 8, 0, 90, color);
    require(rounded_effect_coverage({0, 0}, disk) == 1 && rounded_effect_bounds(disk).width == 10,
            "legacy zero-radius Round behavior changed");
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        const auto zero = make_arc_effect({0, 0}, 0, 8, 0, 90, cap, color);
        require(rounded_effect_coverage({0, 0}, zero) == 0 && rounded_effect_bounds(zero).width == 0,
                "zero-radius explicit non-round arc published coverage");
        const auto empty = make_arc_effect({0, 0}, 10, 4, 0, 0, cap, color);
        require(rounded_effect_coverage({10, 0}, empty) == 0 && rounded_effect_bounds(empty).width == 0,
                "zero sweep retained a non-round cap");
    }
    const auto butt = make_arc_effect({0, 0}, 10, 4, 0, 90, ArcCap::Butt, color);
    const auto square = make_arc_effect({0, 0}, 10, 4, 0, 90, ArcCap::Square, color);
    require(near(distance({10, -1}, butt), 1) && near(distance({10, -1}, square), -1),
            "Butt or Square cap has the wrong tangent extent");
    require(near(distance({10, 0}, butt), 0) && near(distance({10, 0}, square), -2) &&
                rounded_effect_coverage({10, 0}, square) == 1,
            "square cap's internal radial join remained a coverage boundary");
    require(near(distance({13, -3}, square), std::sqrt(2.0F)),
            "square exterior corner distance was approximated with independent masks");
    require(near(rounded_effect_coverage({10, -0.25F}, butt), 0.15625F) &&
                near(rounded_effect_coverage({10, 0.25F}, butt), 0.84375F),
            "Butt end lost continuous antialias coverage");
}

void thick_overlap_and_full_circle() {
    const Color color{1, 0, 0, 0.5F};
    const auto thick = make_arc_effect({0, 0}, 1, 20, 0, 90, ArcCap::Square, color);
    require(near(distance({0, 0}, thick), -10) && rounded_effect_fragment_reference({0, 0}, thick).alpha() == 0.5F,
            "thick intersecting caps retained an internal edge or blended overlap twice");
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        const auto full = make_arc_effect({0, 0}, 1, 20, 0, 360, cap, color);
        require(near(distance({0, 0}, full), -11) && near(distance({12, 0}, full), 1),
                "thick complete circle retained a phantom inner boundary");
    }
    const auto nearly_full = make_arc_effect({0, 0}, 10, 4, 5, 350, ArcCap::Square, color);
    const auto gap = make_arc_effect({0, 0}, 10, 4, 5, 350, ArcCap::Butt, color);
    require(rounded_effect_coverage({10, 0}, nearly_full) == 1 && rounded_effect_coverage({10, 0}, gap) == 0 &&
                rounded_effect_fragment_reference({10, 0}, nearly_full).alpha() == 0.5F,
            "near-full circle lost cap overlap or filled the Butt gap");
    const auto rotated = make_arc_effect({0, 0}, 1, 20, 45, 90, ArcCap::Square, color);
    const auto bounds = rounded_effect_bounds(rotated);
    const auto corner_x = 21 / std::sqrt(2.0F);
    require(bounds.x + bounds.width >= corner_x + 0.999F && rounded_effect_coverage({14, 0.7071F}, rotated, 0.25F) == 1,
            "thick rotated square cap was clipped by radius + half-width bounds");
    const auto translated = make_arc_effect({0, 0}, 1, 20, 45, 90, ArcCap::Square, color, 1, {20, 30});
    require(near(rounded_effect_bounds(translated).x, bounds.x + 20) &&
                near(rounded_effect_coverage({34, 30.7071F}, translated, 0.25F), 1),
            "square cap bounds/coverage lost retained translation");
}

void bounds_enclose_coverage_and_scale() {
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        for (const auto width : {2.0F, 8.0F, 20.0F}) {
            for (const auto sweep : {30.0F, 225.0F, 359.0F}) {
                const auto effect = make_arc_effect({0, 0}, 3, width, 45, sweep, cap, Color{1, 1, 1});
                const auto bounds = rounded_effect_bounds(effect);
                const auto scaled = make_arc_effect({0, 0}, 6, width * 2, 45, sweep, cap, Color{1, 1, 1});
                for (int y = -18; y <= 18; ++y) {
                    for (int x = -18; x <= 18; ++x) {
                        const runtime::Point point{float(x) + 0.125F, float(y) + 0.25F};
                        const auto coverage = rounded_effect_coverage(point, effect);
                        require(near(coverage, rounded_effect_coverage({point.x * 2, point.y * 2}, scaled, 2)),
                                "non-round union is not invariant under logical scaling");
                        if (coverage > 0) {
                            require(point.x >= bounds.x && point.y >= bounds.y && point.x < bounds.x + bounds.width &&
                                        point.y < bounds.y + bounds.height,
                                    "non-round draw bound culled legal antialiased coverage");
                        }
                    }
                }
            }
        }
    }
    try {
        static_cast<void>(make_arc_effect({0, 0}, 10, 2, 0, 90, static_cast<ArcCap>(255), Color{}));
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("unknown ArcCap was accepted");
}
} // namespace

int main() {
    try {
        legacy_and_explicit_caps();
        thick_overlap_and_full_circle();
        bounds_enclose_coverage_and_scale();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
