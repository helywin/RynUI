#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace ryn::detail::effect_linear_exact {

// Signed two's complement, base 2^16. See renderer-contract for the degree-four
// bound. The common quantum is 2^-447 before stop multiplication, then 2^-596.
inline constexpr std::size_t digits = 72;
using Integer = std::array<std::uint32_t, digits>;
template <std::size_t N> using FixedInteger = std::array<std::uint32_t, N>;
using NarrowInteger = FixedInteger<12>;

template <std::size_t N>
inline FixedInteger<N> add(const FixedInteger<N>& a, const FixedInteger<N>& b, bool subtract = false) {
    FixedInteger<N> result{};
    std::uint32_t carry = subtract ? 1 : 0;
    for (std::size_t i = 0; i < N; ++i) {
        const auto term = subtract ? 65535 - b[i] : b[i];
        const auto sum = a[i] + term + carry;
        result[i] = sum & 65535;
        carry = sum >> 16;
    }
    return result;
}

template <std::size_t N> inline FixedInteger<N> shift(const FixedInteger<N>& value, std::uint32_t amount) {
    FixedInteger<N> result{};
    const auto whole = amount / 16;
    const auto part = amount % 16;
    for (std::size_t i = whole; i < N; ++i) {
        result[i] = (value[i - whole] << part) & 65535;
        if (part && i > whole) {
            result[i] |= value[i - whole - 1] >> (16 - part);
        }
    }
    return result;
}

template <std::size_t N> inline FixedInteger<N> times_digit(const FixedInteger<N>& value, std::uint32_t multiplier) {
    FixedInteger<N> result{};
    std::uint32_t carry{};
    for (std::size_t i = 0; i < N; ++i) {
        const auto product = value[i] * multiplier + carry;
        result[i] = product & 65535;
        carry = product >> 16;
    }
    return result;
}

template <std::size_t N>
inline FixedInteger<N> times_significand(const FixedInteger<N>& value, std::uint32_t significand) {
    return add(times_digit(value, significand & 65535), shift(times_digit(value, significand >> 16), 16));
}

inline std::uint32_t significand(std::uint32_t bits) {
    return (bits & 0x7fffff) | ((bits & 0x7f800000) ? 0x800000 : 0);
}

inline std::uint32_t integer_shift(std::uint32_t bits) {
    const auto biased = (bits >> 23) & 255;
    return biased ? biased - 1 : 0;
}

using SmallProduct = std::array<std::uint32_t, 5>;

inline SmallProduct small_times_significand(const SmallProduct& value, std::uint32_t significand) {
    SmallProduct result{};
    for (std::size_t j = 0; j < 2; ++j) {
        const auto multiplier = (significand >> (16 * j)) & 65535;
        std::uint32_t carry{};
        for (std::size_t i = 0; i < result.size() - j; ++i) {
            // Each digit/carry <= 65535: (65535^2)+65535+65535 = 2^32-1.
            const auto product = value[i] * multiplier + result[i + j] + carry;
            result[i + j] = product & 65535;
            carry = product >> 16;
        }
    }
    return result;
}

struct Factor {
    std::uint32_t m{};
    std::int32_t e{};
    std::int32_t l{};
    bool negative{};
};

inline Factor raw_factor(std::uint32_t bits) {
    return {significand(bits), static_cast<std::int32_t>(integer_shift(bits)), 0, (bits >> 31) != 0};
}

// Zero has no exponent contribution and never reaches countr_zero/bit_width.
inline bool decode(std::uint32_t bits, Factor& result) {
    result = {};
    const auto biased = (bits >> 23) & 255;
    if (biased == 255) {
        return false;
    }
    const auto u = significand(bits);
    if (u) {
        const auto z = std::countr_zero(u);
        result.m = u >> z;
        result.e = (biased ? static_cast<std::int32_t>(biased) - 150 : -149) + z;
        result.l = static_cast<std::int32_t>(std::bit_width(result.m));
        result.negative = (bits >> 31) != 0;
    }
    return true;
}

template <std::size_t N> inline FixedInteger<N> factor_product(Factor a, Factor b, Factor c, std::int32_t q) {
    if (!a.m || !b.m || !c.m) {
        return {};
    }
    // Only 24-bit significands enter: stages <2^48 and <2^72. Positive partial
    // rows fit in 80 bits, discarded carries are zero, and the top digit <=255.
    SmallProduct small{};
    small[0] = a.m & 65535;
    small[1] = a.m >> 16;
    small = small_times_significand(small, b.m);
    small = small_times_significand(small, c.m);
    FixedInteger<N> result{};
    for (std::size_t i = 0; i < small.size(); ++i) {
        result[i] = small[i];
    }
    // Raw72: <=3*253. Canonical12: setup proves the shift nonnegative and
    // every term/partial sum strictly inside the signed 192-bit range.
    result = shift(result, static_cast<std::uint32_t>(a.e + b.e + c.e - q));
    return (a.negative != b.negative) != c.negative ? add({}, result, true) : result;
}

inline Integer product(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return factor_product<digits>(raw_factor(a), raw_factor(b), raw_factor(c), 0);
}

template <std::size_t N>
inline void factor_axis(Factor point, Factor start, Factor end, Factor translation, Factor scale, Factor one,
                        std::int32_t q, FixedInteger<N>& n, FixedInteger<N>& d) {
    // N = (P-scale*(T+S))*(E-S); D = scale*(E-S)^2.
    // All terms have quantum 2^-447, including P*E*1 and P*S*1.
    n = add(n, factor_product<N>(point, end, one, q));
    n = add(n, factor_product<N>(point, start, one, q), true);
    n = add(n, factor_product<N>(scale, translation, end, q), true);
    n = add(n, factor_product<N>(scale, translation, start, q));
    n = add(n, factor_product<N>(scale, start, end, q), true);
    n = add(n, factor_product<N>(scale, start, start, q));
    d = add(d, factor_product<N>(scale, end, end, q));
    d = add(d, factor_product<N>(scale, end, start, q), true);
    d = add(d, factor_product<N>(scale, end, start, q), true);
    d = add(d, factor_product<N>(scale, start, start, q));
}

inline void axis(std::uint32_t point, std::uint32_t start, std::uint32_t end, std::uint32_t translation,
                 std::uint32_t scale, Integer& n, Integer& d) {
    // Raw one is (2^23,126), NOT canonical (1,0). Keep the old 2^-447 grid.
    factor_axis(raw_factor(point), raw_factor(start), raw_factor(end), raw_factor(translation), raw_factor(scale),
                raw_factor(0x3f800000), 0, n, d);
}

inline Integer stop_product(const Integer& d, float stop) {
    const auto bits = std::bit_cast<std::uint32_t>(stop);
    auto result = shift(times_significand(d, significand(bits)), integer_shift(bits));
    return (bits >> 31) ? add({}, result, true) : result;
}

template <std::size_t N> inline bool less(const FixedInteger<N>& a, const FixedInteger<N>& b) {
    const auto a_sign = a.back() >> 15;
    const auto b_sign = b.back() >> 15;
    if (a_sign != b_sign) {
        return a_sign != 0;
    }
    for (std::size_t i = N; i-- > 0;) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return false;
}

template <std::size_t N> inline float unit_ratio(FixedInteger<N> remainder, const FixedInteger<N>& denominator) {
    std::uint32_t fraction{};
    for (std::uint32_t i = 0; i < 25; ++i) {
        remainder = shift(remainder, 1);
        fraction <<= 1;
        if (!less(remainder, denominator)) {
            remainder = add(remainder, denominator, true);
            fraction |= 1;
        }
    }
    auto rounded = fraction >> 1;
    if ((fraction & 1) && ((rounded & 1) || less({}, remainder))) {
        ++rounded;
    }
    return static_cast<float>(rounded) * (1.0F / 16777216);
}

// Private sampling seam: only selection is shared with the original color block.
// Coordinates are P.xy, S.xy, E.xy, T.xy, all untouched binary32 bits.
struct Inputs {
    std::array<std::uint32_t, 8> coordinates;
    std::uint32_t scale;
};

struct Selection {
    std::uint32_t next{};
    float fraction{};
};

enum class Failure : std::uint32_t { none, input, setup, denominator, comparison, pair };

struct Setup192 {
    std::array<Factor, 8> coordinates{};
    Factor scale{};
    std::int32_t q{};
    std::int32_t high{};
};

inline Failure setup192(const Inputs& raw, Setup192& result) {
    result = {};
    std::int32_t b{};
    std::int32_t h{1};
    for (std::size_t i = 0; i < raw.coordinates.size(); ++i) {
        auto& value = result.coordinates[i];
        if (!decode(raw.coordinates[i], value)) {
            return Failure::input;
        }
        if (value.m) {
            b = std::min(b, value.e);
            h = std::max(h, value.e + value.l);
        }
    }
    if (!decode(raw.scale, result.scale) || !result.scale.m || result.scale.negative) {
        return Failure::input;
    }
    result.q = 2 * b + std::min(0, result.scale.e);
    result.high = 2 * h + std::max(0, result.scale.e + result.scale.l) + 4;
    return result.high - result.q + 1 <= 192 ? Failure::none : Failure::setup;
}

inline void coordinate192(const Setup192& setup, NarrowInteger& n, NarrowInteger& d) {
    n = {};
    d = {};
    const auto& c = setup.coordinates;
    const Factor one{1, 0, 1, false};
    factor_axis(c[0], c[2], c[4], c[6], setup.scale, one, setup.q, n, d);
    factor_axis(c[1], c[3], c[5], c[7], setup.scale, one, setup.q, n, d);
}

struct StopGrid {
    std::int32_t k{};
    std::int32_t g{};
};

inline void include_stop(StopGrid& grid, Factor stop) {
    if (stop.m) {
        grid.k = std::min(grid.k, stop.e);
        grid.g = std::max(grid.g, stop.e + stop.l);
    }
}

inline bool fits192(const Setup192& setup, StopGrid grid, std::int32_t headroom) {
    return setup.high + grid.g - (setup.q + grid.k) + headroom <= 192;
}

inline NarrowInteger stop192(const NarrowInteger& d, Factor stop, StopGrid grid) {
    if (!stop.m) {
        return {};
    }
    // The comparison/pair gate proves both multiplication and shift fit first.
    auto result = shift(times_significand(d, stop.m), static_cast<std::uint32_t>(stop.e - grid.k));
    return stop.negative ? add({}, result, true) : result;
}

template <class StopAt>
inline bool try_selection192(const Inputs& raw, std::uint32_t count, StopAt stop_at, Selection& selection,
                             Failure& failure) {
    Setup192 setup;
    failure = setup192(raw, setup);
    if (failure != Failure::none) {
        return false;
    }
    NarrowInteger n{};
    NarrowInteger d{};
    coordinate192(setup, n, d);
    if (!less({}, d)) {
        failure = Failure::denominator;
        return false;
    }
    std::uint32_t low{};
    auto high = count;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        Factor stop;
        if (!decode(std::bit_cast<std::uint32_t>(stop_at(middle)), stop)) {
            failure = Failure::input;
            return false;
        }
        StopGrid grid;
        include_stop(grid, stop);
        if (!fits192(setup, grid, 1)) {
            failure = Failure::comparison;
            return false;
        }
        if (less(shift(n, static_cast<std::uint32_t>(-grid.k)), stop192(d, stop, grid))) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    Selection result{low, 0};
    if (low && low < count) {
        Factor previous;
        Factor next;
        if (!decode(std::bit_cast<std::uint32_t>(stop_at(low - 1)), previous) ||
            !decode(std::bit_cast<std::uint32_t>(stop_at(low)), next)) {
            failure = Failure::input;
            return false;
        }
        StopGrid grid;
        include_stop(grid, previous);
        include_stop(grid, next);
        // Mandatory JOINT gate; two successful single-stop gates do not imply it.
        if (!fits192(setup, grid, 3)) {
            failure = Failure::pair;
            return false;
        }
        const auto aligned = shift(n, static_cast<std::uint32_t>(-grid.k));
        const auto start = stop192(d, previous, grid);
        const auto end = stop192(d, next, grid);
        result.fraction = unit_ratio(add(aligned, start, true), add(end, start, true));
    }
    selection = result; // Failed attempts never publish a partial selection.
    return true;
}

template <class StopAt> inline Selection selection_wide(const Inputs& raw, std::uint32_t count, StopAt stop_at) {
    Integer n{};
    Integer d{};
    const auto& c = raw.coordinates;
    axis(c[0], c[2], c[4], c[6], raw.scale, n, d);
    axis(c[1], c[3], c[5], c[7], raw.scale, n, d);
    n = shift(n, 149); // Original raw 72 grid and alignment remain intact.
    std::uint32_t low{};
    auto high = count;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (less(n, stop_product(d, stop_at(middle)))) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    Selection result{low, 0};
    if (low && low < count) {
        const auto start = stop_product(d, stop_at(low - 1));
        const auto end = stop_product(d, stop_at(low));
        result.fraction = unit_ratio(add(n, start, true), add(end, start, true));
    }
    return result;
}

template <class StopAt> inline Selection select(const Inputs& raw, std::uint32_t count, StopAt stop_at) {
    Selection result;
    Failure failure;
    if (try_selection192(raw, count, stop_at, result, failure)) {
        return result;
    }
    // Restart at raw inputs, low=0/high=count; consume no narrow intermediates.
    return selection_wide(raw, count, stop_at);
}

} // namespace ryn::detail::effect_linear_exact
