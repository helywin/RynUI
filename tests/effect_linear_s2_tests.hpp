#pragma once

#include "effect_linear_source_oracle.hpp"
#include "renderer/common/effect_linear_exact.hpp"

#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

namespace effect_linear_s2_tests {
namespace exact = ryn::detail::effect_linear_exact;
namespace oracle = effect_source_oracle;

inline void check(bool value) {
    if (!value) {
        throw std::runtime_error("Linear S2 exact integer/selection/qualification mismatch");
    }
}

inline std::uint32_t bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

template <std::size_t N> inline oracle::Integer integer(const exact::FixedInteger<N>& value) {
    oracle::Integer result;
    for (std::size_t i = N; i-- > 0;) {
        result = oracle::shift(result, 16) + oracle::Integer{value[i]};
    }
    return value.back() & 32768 ? result - oracle::shift(oracle::Integer{1}, 16 * N) : result;
}

inline void equal(const oracle::Integer& a, const oracle::Integer& b) {
    check(oracle::compare(a, b) == 0);
}

inline exact::Inputs inputs(ryn::graphics::EffectLinearDomain domain, ryn::runtime::Point translation, float scale,
                            ryn::runtime::Point point) {
    return {{bits(point.x), bits(point.y), bits(domain.start.x), bits(domain.start.y), bits(domain.end.x),
             bits(domain.end.y), bits(translation.x), bits(translation.y)},
            bits(scale)};
}

inline oracle::Coordinate coordinate(const exact::Inputs& raw) {
    const auto& c = raw.coordinates;
    return oracle::coordinate({{std::bit_cast<float>(c[2]), std::bit_cast<float>(c[3])},
                               {std::bit_cast<float>(c[4]), std::bit_cast<float>(c[5])}},
                              {std::bit_cast<float>(c[6]), std::bit_cast<float>(c[7])}, std::bit_cast<float>(raw.scale),
                              {std::bit_cast<float>(c[0]), std::bit_cast<float>(c[1])});
}

inline exact::Selection source_selection(const oracle::Coordinate& value, const std::vector<float>& stops) {
    const auto n = oracle::shift(value.numerator, 149);
    std::uint32_t next{};
    while (next < stops.size() && oracle::compare(n, oracle::input(stops[next]) * value.denominator) >= 0) {
        ++next;
    }
    exact::Selection result{next, 0};
    if (next && next < stops.size()) {
        const auto start = oracle::input(stops[next - 1]) * value.denominator;
        const auto end = oracle::input(stops[next]) * value.denominator;
        result.fraction = oracle::fraction(n - start, end - start);
    }
    return result;
}

inline void same(exact::Selection a, exact::Selection b) {
    check(a.next == b.next && bits(a.fraction) == bits(b.fraction));
}

inline exact::Failure verify(const exact::Inputs& raw, const std::vector<float>& stops) {
    const auto at = [&](std::uint32_t i) {
        return stops[i];
    };
    const auto count = static_cast<std::uint32_t>(stops.size());
    const auto source = coordinate(raw);
    const auto expected = source_selection(source, stops);
    same(exact::selection_wide(raw, count, at), expected);
    same(exact::select(raw, count, at), expected);
    exact::Selection narrow{0xdeadbeef, -1};
    exact::Failure failure;
    const auto success = exact::try_selection192(raw, count, at, narrow, failure);
    check(success == (failure == exact::Failure::none));
    if (success) {
        same(narrow, expected);
    } else {
        same(narrow, {0xdeadbeef, -1}); // No partial result on failure.
    }
    exact::Setup192 setup;
    if (exact::setup192(raw, setup) == exact::Failure::none) {
        exact::NarrowInteger n{};
        exact::NarrowInteger d{};
        exact::coordinate192(setup, n, d);
        equal(oracle::shift(integer(n), static_cast<unsigned>(setup.q + 447)), source.numerator);
        equal(oracle::shift(integer(d), static_cast<unsigned>(setup.q + 447)), source.denominator);
        // Independently compare every eligible stop product and aligned numerator.
        for (const auto position : stops) {
            exact::Factor stop;
            check(exact::decode(bits(position), stop));
            exact::StopGrid grid;
            exact::include_stop(grid, stop);
            if (exact::fits192(setup, grid, 1)) {
                const auto alignment = static_cast<unsigned>(setup.q + grid.k + 596);
                equal(oracle::shift(integer(exact::stop192(d, stop, grid)), alignment),
                      oracle::input(position) * source.denominator);
                equal(oracle::shift(integer(exact::shift(n, static_cast<std::uint32_t>(-grid.k))), alignment),
                      oracle::shift(source.numerator, 149));
            }
        }
    }
    return failure;
}

inline void qualifications_and_source() {
    using enum exact::Failure;
    for (int k = 90; k <= 93; ++k) {
        for (const bool reverse : {false, true}) {
            const auto y = std::ldexp(1.0F, -k);
            const auto raw = inputs({{reverse ? 1.0F : 0.0F, y}, {reverse ? 0.0F : 1.0F, y}}, {}, 1, {.5F, .5F});
            const std::array expected{none, pair, comparison, setup};
            check(verify(raw, {0, 1}) == expected[k - 90]);
        }
    }
    check(verify(inputs({{0, std::ldexp(1.0F, -90)}, {1, std::ldexp(1.0F, -90)}}, {}, 2, {.5F, .5F}), {0, 1}) == none);
    const auto joint = inputs({{0, std::ldexp(1.0F, -30)}, {1, std::ldexp(1.0F, -30)}}, {}, 1, {.5F, .5F});
    check(verify(joint, {std::ldexp(1.0F, -64), std::ldexp(1.0F, 60)}) == pair);
    exact::Setup192 joint_setup;
    check(exact::setup192(joint, joint_setup) == none && joint_setup.high - joint_setup.q == 67);
    for (const auto value : {std::ldexp(1.0F, -64), std::ldexp(1.0F, 60)}) {
        exact::Factor stop;
        check(exact::decode(bits(value), stop));
        exact::StopGrid grid;
        exact::include_stop(grid, stop);
        check(exact::fits192(joint_setup, grid, 1));
    }
    const auto tiny = std::numeric_limits<float>::denorm_min();
    const auto maximum = std::numeric_limits<float>::max();
    const auto ordinary = inputs({{.5F, 0}, {1.5F, 0}}, {}, 1, {.5F, .5F});
    check(verify(ordinary, {-tiny, maximum}) == pair);
    check(verify(ordinary, {-tiny, tiny}) == none);
    check(verify(ordinary, {-maximum, maximum}) == none);
    // The independent mixed-stop denominator has exactly 277 magnitude bits.
    const auto z = oracle::input(maximum) - oracle::input(-tiny);
    check(z.words.size() == 9 && std::bit_width(z.words.back()) + 256 == 277);
    for (const auto y : {0.0F, -0.0F, tiny, std::bit_cast<float>(0x007fffffu), std::bit_cast<float>(0x00800000u)}) {
        verify(inputs({{-0.0F, y}, {1, y}}, {-tiny, -0.0F}, 1, {.5F, .5F}), {-1, -0.0F, 0, .5F, .5F, 1});
    }
    for (const auto scale : {maximum, tiny, 1.5F}) {
        for (const auto sign : {-1.0F, 1.0F}) {
            check(verify(inputs({{maximum, tiny}, {-maximum, -tiny}}, {sign * maximum, -sign * maximum}, scale,
                                {-sign * maximum, sign * maximum}),
                         {-maximum, -tiny, -0.0F, tiny, maximum}) != none);
        }
    }
    const std::array domains{ryn::graphics::EffectLinearDomain{{-16777211, -16777210}, {-16777203, -16777218}},
                             ryn::graphics::EffectLinearDomain{{-16777210, -16777210}, {-16777202, -16777218}}};
    for (const auto domain : domains) {
        check(verify(inputs(domain, {}, 1.5F, {16.5F, 16.5F}), {0, 1.0F / 32, 1.0F / 32, 1}) == none);
    }
    check(verify(inputs({{33554432, 33554432}, {33554440, 33554424}}, {-33554432, -33554432}, 1, {16.5F, 17.5F}),
                 {-.125F, 0}) == none);
    verify(inputs({{-100000000, -100000000}, {-99999992, -100000008}}, {}, 1, {16.5F, 17.5F}),
           {std::nextafter(-.0625F, -1.0F), std::nextafter(-.0625F, 0.0F)});
    for (const auto& stops : {std::vector<float>{.5F, .5F, .5F}, std::vector<float>{-1, -0.0F, 0, .5F, .5F, 1},
                              std::vector<float>{std::nextafter(.5F, 0.0F), .5F, std::nextafter(.5F, 1.0F)}}) {
        for (const auto x : {-.5F, 0.0F, std::nextafter(.5F, 0.0F), .5F, std::nextafter(.5F, 1.0F), 2.0F}) {
            check(verify(inputs({{0, 0}, {1, 0}}, {}, 1, {x, .5F}), stops) == none);
        }
    }
    std::vector<float> many;
    for (int i = 0; i < 513; ++i) {
        many.push_back(float(i / 2 - 128) / 128);
    }
    check(verify(inputs({{0, 0}, {1, 0}}, {}, 1, {.5F, .5F}), many) == none);
    // Deterministic ordinary full-mantissa box; includes arbitrary CPU sample P.
    std::mt19937 random{0x19272};
    for (int iteration = 0; iteration < 256; ++iteration) {
        exact::Inputs raw;
        for (auto& c : raw.coordinates) {
            c = ((random() & 1u) << 31) | ((115u + random() % 28) << 23) | (random() & 0x7fffffu);
        }
        raw.scale = ((123u + random() % 8) << 23) | (random() & 0x7fffffu);
        std::vector<float> stops{-0.0F, 0};
        for (int i = 0; i < 9; ++i) {
            stops.push_back(std::bit_cast<float>(static_cast<std::uint32_t>(
                ((random() & 1u) << 31) | ((111u + random() % 24) << 23) | (random() & 0x7fffffu))));
        }
        std::ranges::sort(stops);
        check(verify(raw, stops) == none);
    }
    for (const auto invalid : {0u, 0x80000000u, bits(-1), 0x7f800000u, 0x7fc00001u}) {
        auto raw = ordinary;
        raw.scale = invalid;
        exact::Selection result;
        exact::Failure failure;
        check(!exact::try_selection192(raw, 2, [](auto i) { return float(i); }, result, failure) && failure == input);
    }
    auto invalid = ordinary;
    invalid.coordinates[0] = 0x7f800000;
    exact::Setup192 unused;
    check(exact::setup192(invalid, unused) == input);
}

inline void primitives_and_partial_sums() {
    exact::NarrowInteger one{};
    one[0] = 1;
    exact::NarrowInteger all{};
    all.fill(65535);
    check(exact::add(all, one) == exact::NarrowInteger{} && exact::add({}, one, true) == all);
    for (const auto bit : {0u, 1u, 15u, 16u, 17u, 175u, 176u, 190u}) {
        const auto positive = exact::shift(one, bit);
        const auto negative = exact::add({}, positive, true);
        equal(integer(positive), oracle::shift(oracle::Integer{1}, bit));
        equal(integer(negative), -oracle::shift(oracle::Integer{1}, bit));
        equal(integer(exact::add(positive, one, true)), oracle::shift(oracle::Integer{1}, bit) - oracle::Integer{1});
    }
    const auto maximum = exact::add(exact::shift(one, 191), one, true);
    equal(integer(maximum), oracle::shift(oracle::Integer{1}, 191) - oracle::Integer{1});
    equal(integer(exact::add({}, maximum, true)), -(oracle::shift(oracle::Integer{1}, 191) - oracle::Integer{1}));
    for (const auto shift : {0, 1, 15, 16, 17, 118}) {
        for (unsigned signs = 0; signs < 8; ++signs) {
            const exact::Factor a{0xffffff, shift, 24, bool(signs & 1)};
            const exact::Factor b{0xffffff, 0, 24, bool(signs & 2)};
            const exact::Factor c{0xffffff, 0, 24, bool(signs & 4)};
            auto expected =
                oracle::shift(oracle::Integer{0xffffff} * oracle::Integer{0xffffff} * oracle::Integer{0xffffff}, shift);
            if (std::popcount(signs) & 1) {
                expected = -expected;
            }
            equal(integer(exact::factor_product<12>(a, b, c, 0)), expected);
        }
    }
    // Inspect every term and each original-order partial sum independently.
    const auto raw = inputs({{-.25F, 1.125F}, {7.9999995F, -2.25F}}, {.125F, -.5F}, 1.25F, {2.1234567F, -.75F});
    exact::Setup192 setup;
    check(exact::setup192(raw, setup) == exact::Failure::none);
    exact::NarrowInteger n{};
    exact::NarrowInteger d{};
    oracle::Integer expected_n;
    oracle::Integer expected_d;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto& c = setup.coordinates;
        const std::array factors{c[axis],     c[2 + axis], c[4 + axis],
                                 c[6 + axis], setup.scale, exact::Factor{1, 0, 1, false}};
        const std::array raw_values{raw.coordinates[axis],
                                    raw.coordinates[2 + axis],
                                    raw.coordinates[4 + axis],
                                    raw.coordinates[6 + axis],
                                    raw.scale,
                                    bits(1)};
        const std::array<std::array<int, 5>, 10> terms{{{0, 2, 5, 0, 0},
                                                        {0, 1, 5, 1, 0},
                                                        {4, 3, 2, 1, 0},
                                                        {4, 3, 1, 0, 0},
                                                        {4, 1, 2, 1, 0},
                                                        {4, 1, 1, 0, 0},
                                                        {4, 2, 2, 0, 1},
                                                        {4, 2, 1, 1, 1},
                                                        {4, 2, 1, 1, 1},
                                                        {4, 1, 1, 0, 1}}};
        for (const auto& t : terms) {
            const auto value = exact::factor_product<12>(factors[t[0]], factors[t[1]], factors[t[2]], setup.q);
            const auto expected = oracle::input(std::bit_cast<float>(raw_values[t[0]])) *
                                  oracle::input(std::bit_cast<float>(raw_values[t[1]])) *
                                  oracle::input(std::bit_cast<float>(raw_values[t[2]]));
            equal(oracle::shift(integer(value), static_cast<unsigned>(setup.q + 447)), expected);
            auto& sum = t[4] ? d : n;
            auto& expected_sum = t[4] ? expected_d : expected_n;
            sum = exact::add(sum, value, t[3] != 0);
            expected_sum = t[3] ? expected_sum - expected : expected_sum + expected;
            equal(oracle::shift(integer(sum), static_cast<unsigned>(setup.q + 447)), expected_sum);
        }
    }
    exact::NarrowInteger actual_n{};
    exact::NarrowInteger actual_d{};
    exact::coordinate192(setup, actual_n, actual_d);
    check(n == actual_n && d == actual_d);
    for (const auto numerator : {1u, 3u, 5u, 16777217u, 33554431u}) {
        for (const auto denominator : {33554432u, 33554433u}) {
            exact::NarrowInteger r{};
            r[0] = numerator & 65535;
            r[1] = numerator >> 16;
            exact::NarrowInteger z{};
            z[0] = denominator & 65535;
            z[1] = denominator >> 16;
            check(bits(exact::unit_ratio(r, z)) ==
                  bits(oracle::fraction(oracle::Integer{numerator}, oracle::Integer{denominator})));
        }
    }
    const auto z = exact::add(exact::shift(one, 190), one, true);
    for (const auto& r : {exact::NarrowInteger{}, one, exact::add(z, one, true)}) {
        check(bits(exact::unit_ratio(r, z)) == bits(oracle::fraction(integer(r), integer(z))));
        auto remainder = r;
        for (int i = 0; i < 25; ++i) {
            remainder = exact::shift(remainder, 1);
            if (!exact::less(remainder, z)) {
                remainder = exact::add(remainder, z, true);
            }
            check(!exact::less(remainder, exact::NarrowInteger{}) && exact::less(remainder, z));
        }
    }
}

inline void run() {
    primitives_and_partial_sums();
    qualifications_and_source();
}

} // namespace effect_linear_s2_tests
