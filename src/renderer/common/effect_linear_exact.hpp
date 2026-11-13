#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace ryn::detail::effect_linear_exact {

// Signed two's complement, base 2^16. See renderer-contract for the degree-four
// bound. The common quantum is 2^-447 before stop multiplication, then 2^-596.
inline constexpr std::size_t digits = 72;
using Integer = std::array<std::uint32_t, digits>;

inline Integer add(const Integer& a, const Integer& b, bool subtract = false) {
    Integer result{};
    std::uint32_t carry = subtract ? 1 : 0;
    for (std::size_t i = 0; i < digits; ++i) {
        const auto term = subtract ? 65535 - b[i] : b[i];
        const auto sum = a[i] + term + carry;
        result[i] = sum & 65535;
        carry = sum >> 16;
    }
    return result;
}

inline Integer shift(const Integer& value, std::uint32_t amount) {
    Integer result{};
    const auto whole = amount / 16;
    const auto part = amount % 16;
    for (std::size_t i = whole; i < digits; ++i) {
        result[i] = (value[i - whole] << part) & 65535;
        if (part && i > whole) {
            result[i] |= value[i - whole - 1] >> (16 - part);
        }
    }
    return result;
}

inline Integer times_digit(const Integer& value, std::uint32_t multiplier) {
    Integer result{};
    std::uint32_t carry{};
    for (std::size_t i = 0; i < digits; ++i) {
        const auto product = value[i] * multiplier + carry;
        result[i] = product & 65535;
        carry = product >> 16;
    }
    return result;
}

inline Integer times_significand(const Integer& value, std::uint32_t significand) {
    return add(times_digit(value, significand & 65535), shift(times_digit(value, significand >> 16), 16));
}

inline std::uint32_t significand(std::uint32_t bits) {
    return (bits & 0x7fffff) | ((bits & 0x7f800000) ? 0x800000 : 0);
}

inline std::uint32_t integer_shift(std::uint32_t bits) {
    const auto biased = (bits >> 23) & 255;
    return biased ? biased - 1 : 0;
}

inline Integer product(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    Integer result{};
    result[0] = 1;
    for (const auto bits : std::array{a, b, c}) {
        result = times_significand(result, significand(bits));
    }
    result = shift(result, integer_shift(a) + integer_shift(b) + integer_shift(c));
    return ((a ^ b ^ c) >> 31) ? add({}, result, true) : result;
}

inline void axis(std::uint32_t point, std::uint32_t start, std::uint32_t end, std::uint32_t translation,
                 std::uint32_t scale, Integer& n, Integer& d) {
    constexpr auto one = std::bit_cast<std::uint32_t>(1.0F);
    // N = (P-scale*(T+S))*(E-S); D = scale*(E-S)^2.
    // All terms have quantum 2^-447, including P*E*1 and P*S*1.
    n = add(n, product(point, end, one));
    n = add(n, product(point, start, one), true);
    n = add(n, product(scale, translation, end), true);
    n = add(n, product(scale, translation, start));
    n = add(n, product(scale, start, end), true);
    n = add(n, product(scale, start, start));
    d = add(d, product(scale, end, end));
    d = add(d, product(scale, end, start), true);
    d = add(d, product(scale, end, start), true);
    d = add(d, product(scale, start, start));
}

inline Integer stop_product(const Integer& d, float stop) {
    const auto bits = std::bit_cast<std::uint32_t>(stop);
    auto result = shift(times_significand(d, significand(bits)), integer_shift(bits));
    return (bits >> 31) ? add({}, result, true) : result;
}

inline bool less(const Integer& a, const Integer& b) {
    const auto a_sign = a.back() >> 15;
    const auto b_sign = b.back() >> 15;
    if (a_sign != b_sign) {
        return a_sign != 0;
    }
    for (std::size_t i = digits; i-- > 0;) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return false;
}

inline float unit_ratio(Integer remainder, const Integer& denominator) {
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

} // namespace ryn::detail::effect_linear_exact
