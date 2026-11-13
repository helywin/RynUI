#pragma once

#include "graphics/effect_paint.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace effect_source_oracle {

// Test-only unbounded signed-magnitude integers, base 2^32. This does not
// include the packer, packed reference or shader kernel. Source formulas stay
// factored; multiplication is general schoolbook arithmetic, using uint64_t.
struct Integer final {
    std::vector<std::uint32_t> words;
    bool negative{};

    explicit Integer(std::uint32_t value = 0) {
        if (value) {
            words.push_back(value);
        }
    }

    void normalize() {
        while (!words.empty() && !words.back()) {
            words.pop_back();
        }
        if (words.empty()) {
            negative = false;
        }
    }
};

inline int magnitude_compare(const Integer& a, const Integer& b) {
    if (a.words.size() != b.words.size()) {
        return a.words.size() < b.words.size() ? -1 : 1;
    }
    for (std::size_t i = a.words.size(); i-- > 0;) {
        if (a.words[i] != b.words[i]) {
            return a.words[i] < b.words[i] ? -1 : 1;
        }
    }
    return 0;
}

inline int compare(const Integer& a, const Integer& b) {
    if (a.negative != b.negative) {
        return a.negative ? -1 : 1;
    }
    return (a.negative ? -1 : 1) * magnitude_compare(a, b);
}

inline Integer operator-(Integer value) {
    value.negative = !value.words.empty() && !value.negative;
    return value;
}

inline Integer operator+(const Integer& a, const Integer& b) {
    if (a.negative != b.negative && magnitude_compare(a, b) < 0) {
        return b + a;
    }
    Integer result;
    result.negative = a.negative;
    result.words.resize(std::max(a.words.size(), b.words.size()));
    std::uint64_t carry{};
    for (std::size_t i = 0; i < result.words.size(); ++i) {
        const std::uint64_t left = i < a.words.size() ? a.words[i] : 0;
        const std::uint64_t right = i < b.words.size() ? b.words[i] : 0;
        if (a.negative == b.negative) {
            const auto sum = left + right + carry;
            result.words[i] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32;
        } else {
            const auto subtrahend = right + carry;
            result.words[i] = static_cast<std::uint32_t>(left - subtrahend);
            carry = left < subtrahend ? 1 : 0;
        }
    }
    if (a.negative == b.negative && carry) {
        result.words.push_back(static_cast<std::uint32_t>(carry));
    }
    result.normalize();
    return result;
}

inline Integer operator-(const Integer& a, const Integer& b) {
    return a + -b;
}

inline Integer operator*(const Integer& a, const Integer& b) {
    Integer result;
    result.negative = a.negative != b.negative;
    result.words.resize(a.words.size() + b.words.size());
    for (std::size_t i = 0; i < a.words.size(); ++i) {
        std::uint64_t carry{};
        for (std::size_t j = 0; j < b.words.size(); ++j) {
            const auto product = std::uint64_t(a.words[i]) * b.words[j] + result.words[i + j] + carry;
            result.words[i + j] = static_cast<std::uint32_t>(product);
            carry = product >> 32;
        }
        result.words[i + b.words.size()] = static_cast<std::uint32_t>(carry);
    }
    result.normalize();
    return result;
}

inline Integer shift(const Integer& value, unsigned amount) {
    Integer power;
    power.words.resize(amount / 32 + 1);
    power.words.back() = std::uint32_t{1} << (amount % 32);
    return value * power;
}

// Returns the exact integer f * 2^149, retaining subnormal and signed inputs.
inline Integer input(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto exponent = (bits >> 23) & 255;
    if (exponent == 255) {
        throw std::invalid_argument("source oracle requires finite binary32 input");
    }
    auto result = shift(Integer{(bits & 0x7fffff) | (exponent ? 0x800000 : 0)}, exponent ? exponent - 1 : 0);
    result.negative = (bits >> 31) && !result.words.empty();
    return result;
}

struct Coordinate final {
    Integer numerator;
    Integer denominator;
};

inline Coordinate coordinate(ryn::graphics::EffectLinearDomain domain, ryn::runtime::Point translation, float scale,
                             ryn::runtime::Point point_pixels) {
    const auto ax = input(domain.end.x) - input(domain.start.x);
    const auto ay = input(domain.end.y) - input(domain.start.y);
    const auto s = input(scale);
    const auto qx = shift(input(point_pixels.x), 149) - s * (input(translation.x) + input(domain.start.x));
    const auto qy = shift(input(point_pixels.y), 149) - s * (input(translation.y) + input(domain.start.y));
    Coordinate result{qx * ax + qy * ay, s * (ax * ax + ay * ay)};
    if (compare(result.denominator, Integer{}) <= 0) {
        throw std::invalid_argument("source oracle requires a nondegenerate Linear domain and positive scale");
    }
    return result;
}

// Independent integer division by binary search, then exact residual tie test.
// Rounds to the shader's stated 2^-24 fraction grid, ties to even.
inline float fraction(const Integer& numerator, const Integer& denominator) {
    if (compare(numerator, Integer{}) < 0 || compare(numerator, denominator) >= 0) {
        throw std::invalid_argument("source oracle fraction outside [0,1)");
    }
    const auto scaled = shift(numerator, 24);
    std::uint32_t low{};
    std::uint32_t high = 16777216;
    while (low < high) {
        const auto middle = low + (high - low + 1) / 2;
        if (compare(denominator * Integer{middle}, scaled) <= 0) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    const auto remainder = scaled - denominator * Integer{low};
    const auto tie = compare(shift(remainder, 1), denominator);
    if (tie > 0 || (tie == 0 && (low & 1))) {
        ++low;
    }
    return static_cast<float>(low) / 16777216;
}

inline std::array<double, 4> premultiplied(ryn::Color color) {
    return {double(color.red()) * color.alpha(), double(color.green()) * color.alpha(),
            double(color.blue()) * color.alpha(), color.alpha()};
}

inline std::array<double, 4> sample(const ryn::graphics::EffectPaintField& field, ryn::runtime::Point translation,
                                    float scale, ryn::runtime::Point point_pixels) {
    const auto value =
        coordinate(std::get<ryn::graphics::EffectLinearDomain>(field.domain()), translation, scale, point_pixels);
    const auto n = shift(value.numerator, 149);
    const auto stops = field.ramp().stops();
    std::size_t next{};
    // Linear scan is intentionally independent of the production upper_bound.
    while (next < stops.size() && compare(n, input(stops[next].position) * value.denominator) >= 0) {
        ++next;
    }
    const auto& previous = stops[next ? next - 1 : 0];
    auto color = premultiplied(previous.color);
    if (next && next < stops.size()) {
        const auto start = input(previous.position) * value.denominator;
        const auto end = input(stops[next].position) * value.denominator;
        const double t = fraction(n - start, end - start);
        const auto last = premultiplied(stops[next].color);
        for (std::size_t channel = 0; channel < color.size(); ++channel) {
            color[channel] += t * (last[channel] - color[channel]);
        }
    }
    return color;
}

} // namespace effect_source_oracle
