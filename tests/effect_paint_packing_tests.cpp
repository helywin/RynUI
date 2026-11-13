#include "graphics/color_field.hpp"
#include "renderer/common/effect_paint_packing.hpp"
#include "renderer/common/effect_linear_exact.hpp"
#include "effect_linear_source_oracle.hpp"
#include "effect_linear_s2_tests.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid packed paint accepted");
}

EffectPaintField linear(Color first, Color last) {
    return {EffectGradientRamp{{0, first}, {1, last}}, EffectLinearDomain{{10, 10}, {30, 10}}};
}

RoundedEffectInstance fill() {
    return make_shadow_effect({{10, 10, 20, 20}, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 1, 1}});
}

void compare(const RoundedEffectInstance& effect, runtime::Point point, float scale) {
    EffectPaintPackedTables tables;
    const std::array source{effect};
    const SceneDeviceMetrics metrics{200, 200, scale};
    pack_effect_paint_tables(source, metrics, tables);
    const auto base = pack_effect_paint_base_instance(effect, metrics);
    const auto cap = pack_cap_geometry(effect, metrics);
    const auto packed = effect_paint_gpu_fragment_reference({point.x * scale, point.y * scale}, base,
                                                            tables.links.front(), tables.fields, tables.stops, &cap);
    // This modest-input compatibility check uses the old approximate float-local
    // reference. Exact-affine source cases below have an independent oracle.
    const auto expected = rounded_effect_fragment_reference(point, effect, 1 / scale);
    const std::array color{expected.red(), expected.green(), expected.blue(), expected.alpha()};
    for (std::size_t channel = 0; channel < color.size(); ++channel) {
        // A fully transparent fragment has no observable straight RGB.
        if (channel == 3 || (color[3] > 0 && packed[3] > 0)) {
            require(std::abs(packed[channel] - color[channel]) < 0.00005F,
                    "packed table reference differs from the logical reference");
        }
    }
}

void layout_bytes_and_domains() {
    auto value = fill();
    const EffectGradientRamp ramp{
        {-1, Color{1, 0, 0, 0}}, {0.5F, Color{0, 1, 0, 0.5F}}, {0.5F, Color{0, 0, 1, 1}}, {2, Color{1, 1, 1}}};
    value.material.paint =
        EffectPaint{{ramp, EffectLinearDomain{{10, 10}, {30, 10}}}, linear(Color{1, 0, 0, 0.5F}, Color{1, 0, 0, 0.5F})};
    value.geometry.translation = {3, 7};
    EffectPaintPackedTables tables;
    const std::array source{value};
    pack_effect_paint_tables(source, {200, 200, 2}, tables);
    require(tables.links == std::vector<EffectPaintGpuLink>{{0, 1, 0, 0}} && tables.fields.size() == 2 &&
                tables.stops.size() == 6 &&
                tables.fields[0].raw_domain_bits ==
                    std::bit_cast<std::array<std::uint32_t, 4>>(std::array<float, 4>{10, 10, 30, 10}) &&
                tables.fields[0].affine_bits ==
                    std::bit_cast<std::array<std::uint32_t, 4>>(std::array<float, 4>{3, 7, 2, 0}) &&
                tables.fields[1].first_stop == 4 && tables.fields[0].stop_count == 4 &&
                tables.stops[0].position == std::array<float, 4>{-1, 0, 0, 0} &&
                tables.stops[0].premultiplied_rgba == std::array<float, 4>{0, 0, 0, 0} &&
                tables.stops[1].premultiplied_rgba == std::array<float, 4>{0, 0.5F, 0, 0.5F},
            "paint ABI field offsets, span, translation, scale or premultiplied bytes differ");
    for (const auto scale : {1.0F, 1.25F, 2.0F}) {
        for (const auto x : {13.0F, 22.9F, 23.0F, 23.1F, 32.75F}) {
            compare(value, {x, 20}, scale);
        }
    }
    // No arbitrary small stop limit, including duplicate/out-of-domain positions.
    std::vector<EffectGradientStop> many;
    for (int index = 0; index < 257; ++index) {
        many.push_back({float(index) / 256, Color{0, 0, 1, 0.5F}});
    }
    value.material.paint =
        EffectPaint{{EffectGradientRamp{many}, EffectLinearDomain{{10, 10}, {30, 10}}}, std::nullopt};
    pack_effect_paint_tables(std::array{value}, {200, 200, 1}, tables);
    require(tables.fields.front().stop_count == 257 && tables.stops.size() == 257,
            "paint pack imposed a small stop limit");
}

void composition_caps_and_legacy_parameters() {
    auto value = fill();
    value.material.opacity = 0.5F;
    value.material.paint = EffectPaint{linear(Color{1, 0, 0, 0.5F}, Color{1, 0, 0, 0.5F}),
                                       linear(Color{0, 0, 1, 0.5F}, Color{0, 0, 1, 0.5F})};
    EffectPaintPackedTables tables;
    pack_effect_paint_tables(std::array{value}, {100, 100, 1}, tables);
    const auto sample = effect_paint_gpu_fragment_reference(
        {20, 20}, pack_effect_paint_base_instance(value, {100, 100, 1}), tables.links[0], tables.fields, tables.stops);
    require(sample[3] == 0.375F && std::abs(sample[0] - 2.0F / 3) < 0.00001F &&
                std::abs(sample[2] - 1.0F / 3) < 0.00001F,
            "dual fields applied opacity before source-over");
    const EffectGradientRamp ramp{{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1, 1}}};
    value.material.paint->foreground =
        EffectPaintField{ramp, EffectConicDomain{{20, 20}, 0, 2 * std::numbers::pi_v<float>}};
    for (const auto point : {runtime::Point{20, 20}, {21, 20}, {20, 21}, {19, 20}, {20, 19}}) {
        compare(value, point, 1.25F);
    }
    for (const auto cap : {ArcCap::Round, ArcCap::Butt, ArcCap::Square}) {
        for (const auto width : {4.0F, 20.0F}) {
            for (const auto sweep : {90.0F, 350.0F, 360.0F}) {
                auto arc = make_arc_effect({25, 25}, 3, width, 5, sweep, cap, Color{1, 0, 0, 0.5F});
                if (cap == ArcCap::Round) {
                    arc.material.paint = value.material.paint;
                }
                for (const auto point : {runtime::Point{25, 25}, {28, 25}, {28, 24.75F}, {35, 20}, {40, 25}}) {
                    compare(arc, point, 2);
                }
            }
        }
    }
    const std::array legacy{
        make_dashed_outline_effect({{10, 10, 30, 20}, 3}, 2, {3, 2, 1}, Color{1, 0, 0}),
        make_tooltip_fill_effect({{10, 10, 30, 20}, 3}, {TooltipArrowSide::top, 15, 3, 4}, Color{0, 1, 0})};
    for (auto effect : legacy) {
        const auto base = pack_rounded_effect_instance(effect, {100, 100, 1});
        effect.material.paint = value.material.paint;
        const auto painted_base = pack_effect_paint_base_instance(effect, {100, 100, 1});
        require(base == painted_base, "paint reused base dash/Tooltip parameter slots");
        compare(effect, {25, 10}, 1.25F);
    }
}

void half_open_solid_field_with_paint() {
    for (const auto scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const auto phase : {0.0F, 0.25F, 0.5F, 0.75F}) {
            const runtime::Rect bounds{12 + phase / scale, 16 + phase / scale, 36, 14};
            auto source = sampled_color_field(
                bounds, {}, 8, 4, [](float, float) { return Color{0.2F, 0.4F, 0.6F, 0.5F}; }, {1, {0, 0, 100, 80}});
            auto painted = fill();
            painted.geometry.translation = {60, 0};
            painted.material.paint = EffectPaint{linear(Color{1, 0, 0}, Color{0, 0, 1}), std::nullopt};
            source.push_back(painted);
            EffectPaintPackedTables tables;
            pack_effect_paint_tables(source, {200, 160, scale}, tables);
            require(tables.links.size() == source.size() && tables.links.back().foreground_field == 0,
                    "mixed ColorPicker solid field/paint link indices differ");
            // Every cell boundary has one owner. A new paint sharing this scene
            // must leave old solidEffects' half-open coverage and bytes intact.
            const runtime::Point boundary{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            float alpha{};
            for (std::size_t index = 0; index + 1 < source.size(); ++index) {
                const auto old = pack_rounded_effect_instance(source[index], {200, 160, scale});
                const auto base = pack_effect_paint_base_instance(source[index], {200, 160, scale});
                require(old == base && tables.links[index].foreground_field == effect_paint_no_field,
                        "paint altered solid field base bytes or assigned it a field");
                alpha += effect_paint_gpu_fragment_reference({boundary.x * scale, boundary.y * scale}, base,
                                                             tables.links[index], tables.fields, tables.stops)[3];
            }
            require(std::abs(alpha - 0.5F) < 0.00001F, "solid field half-open boundary blended twice or left a seam");
        }
    }
}

void malformed_tables_and_capacity() {
    auto value = fill();
    value.material.paint = EffectPaint{linear(Color{1, 0, 0}, Color{0, 0, 1}), std::nullopt};
    EffectPaintPackedTables tables;
    pack_effect_paint_tables(std::array{value}, {100, 100, 1}, tables);
    for (int mode = 0; mode < 12; ++mode) {
        auto bad = tables;
        if (mode == 0) {
            bad.links[0].foreground_field = 1;
        } else if (mode == 1) {
            bad.links[0].arc_cap = 3;
        } else if (mode == 2) {
            bad.fields[0].first_stop = effect_paint_no_field;
        } else if (mode == 3) {
            bad.fields[0].stop_count = effect_paint_no_field;
        } else if (mode == 4) {
            bad.fields[0].raw_domain_bits[2] = bad.fields[0].raw_domain_bits[0];
        } else if (mode == 5) {
            bad.stops[0].position[0] = 2;
        } else if (mode == 6) {
            bad.stops[0].premultiplied_rgba[3] = 0;
        } else if (mode == 7) {
            bad.links[0].reserved = 1;
        } else if (mode == 8) {
            bad.fields[0].affine_bits[2] = 0;
        } else if (mode == 9) {
            bad.fields[0].affine_bits[2] = std::bit_cast<std::uint32_t>(-1.0F);
        } else if (mode == 10) {
            bad.fields[0].affine_bits[0] = 0x7f800000;
        } else {
            bad.fields[0].affine_bits[3] = 1;
        }
        rejects([&] { validate_effect_paint_link(bad.links[0], bad.fields, bad.stops); });
    }
    require(effect_buffer_capacity(3, 0, 32, 128) == 4 && effect_buffer_capacity(1, 8, 32, 256) == 8,
            "capacity calculation differs from actual allocation growth");
    rejects([] { static_cast<void>(effect_buffer_capacity(3, 0, 32, 127)); });
    rejects([] { static_cast<void>(effect_buffer_capacity(1, 8, 32, 255)); });
    require(effect_buffer_capacity(3, 0, effect_paint_field_stride, 192) == 4,
            "raw field capacity did not use its declared 48-byte stride");
    rejects([] { static_cast<void>(effect_buffer_capacity(3, 0, effect_paint_field_stride, 191)); });
    rejects([] {
        static_cast<void>(effect_buffer_capacity(2, 0, std::numeric_limits<std::size_t>::max(),
                                                 std::numeric_limits<std::uint64_t>::max()));
    });
    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
        rejects([] {
            static_cast<void>(effect_buffer_capacity(std::size_t(effect_paint_no_field) + 1, 0, 16,
                                                     std::numeric_limits<std::uint64_t>::max()));
        });
    }
}

void raw_affine_source_regressions() {
    const EffectGradientRamp discontinuous{
        {0, Color{1, 0, 0}}, {1.0F / 32, Color{1, 0, 0}}, {1.0F / 32, Color{0, 0, 1}}, {1, Color{0, 0, 1}}};
    const EffectLinearDomain a{{-16777211, -16777210}, {-16777203, -16777218}};
    const EffectLinearDomain b{{-16777210, -16777210}, {-16777202, -16777218}};
    const auto old_domain = [](EffectLinearDomain domain) {
        return std::array{domain.start.x * 1.5F, domain.start.y * 1.5F, domain.end.x * 1.5F, domain.end.y * 1.5F};
    };
    require(old_domain(a) == old_domain(b), "DPI collision fixture does not collide in v1");
    const auto first = pack_effect_paint_field({discontinuous, a}, {}, {128, 96, 1.5F});
    const auto second = pack_effect_paint_field({discontinuous, b}, {}, {128, 96, 1.5F});
    require(first != second && first.raw_domain_bits != second.raw_domain_bits &&
                first.affine_bits == second.affine_bits,
            "raw affine ABI retained the source-to-packed collision");

    const auto verify = [&](EffectPaintField field, runtime::Point translation, float scale, runtime::Point point,
                            std::optional<std::array<double, 4>> analytic = std::nullopt) {
        auto effect = fill();
        effect.geometry.translation = translation;
        effect.material.paint = EffectPaint{field, std::nullopt};
        EffectPaintPackedTables tables;
        pack_effect_paint_tables(std::array{effect}, {128, 96, scale}, tables);
        // Use an independent interior surface so only the paint is queried.
        const auto base = pack_rounded_effect_instance(fill(), {128, 96, 1});
        const auto actual =
            effect_paint_gpu_fragment_reference(point, base, tables.links[0], tables.fields, tables.stops);
        const auto expected = effect_source_oracle::sample(field, translation, scale, point);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            if (analytic) {
                require(std::abs(expected[channel] - (*analytic)[channel]) < 1e-7,
                        "independent source oracle disagrees with analytic fixture");
            }
            const auto pm = channel == 3 ? actual[3] : actual[channel] * actual[3];
            require(std::abs(double(pm) - expected[channel]) < 2e-7,
                    "packed Linear differs from independent exact-affine source oracle");
        }
    };
    verify({discontinuous, a}, {}, 1.5F, {16.5F, 16.5F}, std::array<double, 4>{0, 0, 1, 1});
    verify({discontinuous, b}, {}, 1.5F, {16.5F, 16.5F}, std::array<double, 4>{1, 0, 0, 1});
    const EffectGradientRamp blend{{-0.125F, Color{1, 0, 0}}, {0, Color{0, 0, 1}}};
    verify({blend, EffectLinearDomain{{33554432, 33554432}, {33554440, 33554424}}}, {-33554432, -33554432}, 1,
           {16.5F, 17.5F}, std::array<double, 4>{0.5, 0, 0.5, 1});
    const auto tiny = std::numeric_limits<float>::denorm_min();
    const auto maximum = std::numeric_limits<float>::max();
    for (const auto ramp : {EffectGradientRamp{{-maximum, Color{1, 0, 0}}, {maximum, Color{0, 0, 1}}},
                            EffectGradientRamp{{-tiny, Color{1, 0, 0}}, {tiny, Color{0, 0, 1}}}}) {
        verify({ramp, EffectLinearDomain{{16.5F, -maximum / 4}, {17.5F, -maximum / 4}}}, {}, 1, {16.5F, 16.5F},
               std::array<double, 4>{0.5, 0, 0.5, 1});
    }
    const EffectGradientRamp unit{{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1, 1}}};
    verify({unit, EffectLinearDomain{{0, 0}, {std::ldexp(1.0F, 60), 0}}}, {}, std::ldexp(1.0F, -60), {16.5F, 16.5F},
           std::array<double, 4>{0, 0, 1, 1});
    verify({unit, EffectLinearDomain{{-0.0F, tiny}, {32, tiny}}}, {-tiny, 0}, 1, {16.5F, 16.5F});
    const auto raw =
        pack_effect_paint_field({unit, EffectLinearDomain{{-0.0F, tiny}, {32, tiny}}}, {-tiny, -0.0F}, {128, 96, 1});
    require(raw.raw_domain_bits[0] == 0x80000000 && raw.raw_domain_bits[1] == 1 && raw.affine_bits[0] == 0x80000001 &&
                raw.affine_bits[1] == 0x80000000,
            "raw field load lost signed zero or subnormal bits");
    const auto before = std::nextafter(-0.0625F, -1.0F);
    const auto after = std::nextafter(-0.0625F, 0.0F);
    verify({EffectGradientRamp{{before, Color{1, 0, 0}}, {after, Color{0, 0, 1}}},
            EffectLinearDomain{{-100000000, -100000000}, {-99999992, -100000008}}},
           {}, 1, {16.5F, 17.5F});
    rejects([&] {
        static_cast<void>(pack_effect_paint_field({unit, EffectLinearDomain{{0, 0}, {tiny, 0}}}, {}, {128, 96, 1}));
    });
}

void exact_ratio_guard_sticky_and_carry() {
    using namespace effect_linear_exact;
    for (const auto numerator : {1u, 3u, 5u, 16777217u, 33554431u}) {
        Integer n{};
        n[0] = numerator & 65535;
        n[1] = numerator >> 16;
        Integer d{};
        d[1] = 512; // 2^25; odd n exercises exact ties, both parity choices.
        const auto expected = effect_source_oracle::fraction(effect_source_oracle::Integer{numerator},
                                                             effect_source_oracle::Integer{33554432});
        require(unit_ratio(n, d) == expected, "unit ratio guard/tie-even differs from independent division");
        d[0] = 1; // Nonzero residual is the sticky bit.
        require(unit_ratio(n, d) == effect_source_oracle::fraction(effect_source_oracle::Integer{numerator},
                                                                   effect_source_oracle::Integer{33554433}),
                "unit ratio sticky rounding differs from independent division");
    }
    Integer all{};
    all.fill(65535);
    Integer one{};
    one[0] = 1;
    require(add(all, one) == Integer{} && add({}, one, true) == all,
            "base16 carry or two's-complement borrow chain differs");
    require(shift(one, 1151).back() == 32768 && shift(one, 1152) == Integer{}, "wide shift length differs");
}

effect_source_oracle::Integer decode_exact_integer(const effect_linear_exact::Integer& value) {
    effect_source_oracle::Integer result;
    for (std::size_t i = effect_linear_exact::digits; i-- > 0;) {
        result = effect_source_oracle::shift(result, 16) + effect_source_oracle::Integer{value[i]};
    }
    if (value.back() & 32768) {
        result = result - effect_source_oracle::shift(effect_source_oracle::Integer{1}, 1152);
    }
    return result;
}

void exact_sparse_product_boundaries() {
    using namespace effect_linear_exact;
    const auto verify = [](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const auto expected = effect_source_oracle::input(std::bit_cast<float>(a)) *
                              effect_source_oracle::input(std::bit_cast<float>(b)) *
                              effect_source_oracle::input(std::bit_cast<float>(c));
        require(effect_source_oracle::compare(decode_exact_integer(product(a, b, c)), expected) == 0,
                "small significand product lost a carry, exponent bit, zero or sign");
    };
    // Raw exponents exercise shifts 0,1,15,16,17,253 and their sums. Include
    // +/-0, min/max subnormals, the normal boundary, all-one limbs and negatives.
    const std::array inputs{0u,          0x80000000u, 1u,          0x80000001u, 0x007fffffu, 0x00800000u, 0x00ffffffu,
                            0x01000001u, 0x0800ffffu, 0x0880ffffu, 0x0900ffffu, 0x7f7fffffu, 0xff7fffffu};
    for (const auto a : inputs) {
        for (const auto b : inputs) {
            for (const auto c : inputs) {
                verify(a, b, c);
            }
        }
    }
    for (std::uint32_t signs = 0; signs < 8; ++signs) {
        verify(0x00ffffffu | ((signs & 1u) << 31), 0x00ffffffu | (((signs >> 1) & 1u) << 31),
               0x00ffffffu | (((signs >> 2) & 1u) << 31));
    }
    // Independently expand (2^24-1)^3 = 2^72-3*2^48+3*2^24-1.
    const auto expected_cube = effect_source_oracle::shift(effect_source_oracle::Integer{1}, 72) -
                               effect_source_oracle::shift(effect_source_oracle::Integer{3}, 48) +
                               effect_source_oracle::shift(effect_source_oracle::Integer{3}, 24) -
                               effect_source_oracle::Integer{1};
    const auto cube = product(0x00ffffffu, 0x00ffffffu, 0x00ffffffu);
    require(effect_source_oracle::compare(decode_exact_integer(cube), expected_cube) == 0 && cube[4] == 255 &&
                std::all_of(cube.begin() + 5, cube.end(), [](auto digit) { return digit == 0; }),
            "maximum triple significand did not retain its full 72-bit product");
    Integer one{};
    one[0] = 1;
    require(effect_source_oracle::compare(decode_exact_integer(add(cube, one)),
                                          expected_cube + effect_source_oracle::Integer{1}) == 0 &&
                effect_source_oracle::compare(decode_exact_integer(add(one, cube, true)),
                                              effect_source_oracle::Integer{1} - expected_cube) == 0,
            "small product expansion lost wide accumulation carry or subtraction");
}

void raw_affine_dual_source_regressions() {
    const EffectLinearDomain a{{-16777211, -16777210}, {-16777203, -16777218}};
    const EffectLinearDomain b{{-16777210, -16777210}, {-16777202, -16777218}};
    for (const auto alpha : {0.0F, 0.5F, std::nextafter(1.0F, 0.0F), 1.0F}) {
        auto effect = fill();
        const EffectGradientRamp foreground{{0, Color{1, 0, 0, alpha}},
                                            {1.0F / 32, Color{1, 0, 0, alpha}},
                                            {1.0F / 32, Color{0, 0, 1, alpha}},
                                            {1, Color{0, 0, 1, alpha}}};
        const EffectGradientRamp underlay{{0, Color{0, 1, 0, 0.5F}}, {1, Color{1, 1, 0, 0.5F}}};
        effect.material.paint = EffectPaint{{foreground, a}, EffectPaintField{underlay, b}};
        EffectPaintPackedTables tables;
        pack_effect_paint_tables(std::array{effect}, {128, 96, 1.5F}, tables);
        const runtime::Point point{16.5F, 16.5F};
        auto expected = effect_source_oracle::sample(effect.material.paint->foreground, {}, 1.5F, point);
        const auto behind = effect_source_oracle::sample(*effect.material.paint->underlay, {}, 1.5F, point);
        require(expected == std::array<double, 4>{0, 0, alpha, alpha} &&
                    behind == std::array<double, 4>{0, 0.5, 0, 0.5},
                "independent L1 dual-field analytic colors differ");
        const auto remaining = 1 - expected[3];
        for (std::size_t channel = 0; channel < expected.size(); ++channel) {
            expected[channel] += behind[channel] * remaining;
        }
        for (const auto opacity : {0.0F, 0.5F, 1.0F}) {
            auto surface = fill();
            surface.material.opacity = opacity;
            // The interior, exact AA midpoint and outside have coverage 1,.5,0.
            for (const auto left : {10.0F, 16.5F, 20.0F}) {
                surface.geometry.shape.rect.x = left;
                const auto coverage = left == 10 ? 1.0 : left == 16.5F ? 0.5 : 0.0;
                const auto actual =
                    effect_paint_gpu_fragment_reference(point, pack_rounded_effect_instance(surface, {128, 96, 1}),
                                                        tables.links[0], tables.fields, tables.stops);
                if (alpha < 1 && opacity > 0 && coverage > 0) {
                    require(actual[1] > 0, "nonopaque foreground lost the distinct underlay, including nextdown(1)");
                }
                for (std::size_t channel = 0; channel < expected.size(); ++channel) {
                    const auto pm = channel == 3 ? actual[3] : actual[channel] * actual[3];
                    require(std::abs(double(pm) - expected[channel] * coverage * opacity) < 2e-7,
                            "dual fields changed source-over or coverage/opacity applied once");
                }
            }
        }
    }
}

void exact_polynomial_integer_boundaries() {
    using namespace effect_linear_exact;
    const auto maximum = std::numeric_limits<float>::max();
    const auto tiny = std::numeric_limits<float>::denorm_min();
    // Arithmetic kernel probes intentionally include points outside the retained
    // v1 admission envelope. They test the full finite-input polynomial bound,
    // without claiming the packer or a GPU draw accepts these scenes.
    for (const auto scale : {maximum, tiny, 1.5F}) {
        for (const auto sign : {-1.0F, 1.0F}) {
            const EffectLinearDomain domain{{maximum, tiny}, {-maximum, -tiny}};
            const runtime::Point translation{sign * maximum, -sign * maximum};
            const runtime::Point point{-sign * maximum, sign * maximum};
            const auto source = effect_source_oracle::coordinate(domain, translation, scale, point);
            Integer n{};
            Integer d{};
            axis(std::bit_cast<std::uint32_t>(point.x), std::bit_cast<std::uint32_t>(domain.start.x),
                 std::bit_cast<std::uint32_t>(domain.end.x), std::bit_cast<std::uint32_t>(translation.x),
                 std::bit_cast<std::uint32_t>(scale), n, d);
            axis(std::bit_cast<std::uint32_t>(point.y), std::bit_cast<std::uint32_t>(domain.start.y),
                 std::bit_cast<std::uint32_t>(domain.end.y), std::bit_cast<std::uint32_t>(translation.y),
                 std::bit_cast<std::uint32_t>(scale), n, d);
            require(effect_source_oracle::compare(decode_exact_integer(n), source.numerator) == 0 &&
                        effect_source_oracle::compare(decode_exact_integer(d), source.denominator) == 0,
                    "1152-bit original-factor polynomial differs from unbounded factored source");
            for (const auto stop : {-maximum, -tiny, -0.0F, tiny, maximum}) {
                const auto expected = effect_source_oracle::input(stop) * source.denominator;
                const auto actual = stop_product(d, stop);
                require(effect_source_oracle::compare(decode_exact_integer(actual), expected) == 0 &&
                            less(shift(n, 149), actual) ==
                                (effect_source_oracle::compare(effect_source_oracle::shift(source.numerator, 149),
                                                               expected) < 0),
                        "degree-four stop product/sign lost high or low bits");
            }
        }
    }
}
} // namespace

int main() {
    try {
        layout_bytes_and_domains();
        composition_caps_and_legacy_parameters();
        half_open_solid_field_with_paint();
        malformed_tables_and_capacity();
        raw_affine_source_regressions();
        exact_ratio_guard_sticky_and_carry();
        exact_sparse_product_boundaries();
        raw_affine_dual_source_regressions();
        exact_polynomial_integer_boundaries();
        effect_linear_s2_tests::run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
