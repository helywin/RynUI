#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ryn_test::composition_reference {
using Pixel = std::array<double, 4>;
using Image = std::vector<Pixel>;

inline double quantized(double value) {
    return std::round(std::clamp(value, 0.0, 1.0) * 255.0) / 255.0;
}

inline Pixel stored(Pixel value) {
    for (auto& c : value) {
        c = quantized(c);
    }
    return value;
}

// Independent double oracle. Does not call production packers, SDFs, weights or shaders.
// Both separable passes store RGBA8, including alpha, just like the declared native target format.
inline Image gaussian(const Image& input, unsigned width, unsigned height, double sigma) {
    if (sigma == 0) {
        return input;
    }
    const int radius = int(std::ceil(3 * sigma));
    std::vector<double> weights(std::size_t(radius) * 2 + 1);
    double sum = 0;
    for (int i = -radius; i <= radius; ++i) {
        const double w = std::exp(-double(i) * i / (2 * sigma * sigma));
        weights[std::size_t(i + radius)] = w;
        sum += w;
    }
    for (auto& w : weights) {
        w /= sum;
    }
    Image source = input;
    Image target(source.size());
    for (unsigned axis = 0; axis < 2; ++axis) {
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                Pixel total{};
                for (int tap = -radius; tap <= radius; ++tap) {
                    const unsigned sx = unsigned(std::clamp(int(x) + (axis == 0 ? tap : 0), 0, int(width) - 1));
                    const unsigned sy = unsigned(std::clamp(int(y) + (axis == 1 ? tap : 0), 0, int(height) - 1));
                    const auto& sample = source[std::size_t(sy) * width + sx];
                    for (unsigned c = 0; c < 4; ++c) {
                        total[c] += sample[c] * weights[std::size_t(tap + radius)];
                    }
                }
                target[std::size_t(y) * width + x] = stored(total);
            }
        }
        source.swap(target);
    }
    return source;
}

inline Pixel replacement(Pixel b, Pixel f, Pixel tint, double m) {
    Pixel result;
    for (unsigned c = 0; c < 4; ++c) {
        const double e = tint[c] + f[c] * (1 - tint[3]);
        result[c] = (1 - m) * b[c] + m * e;
    }
    return stored(result);
}
} // namespace ryn_test::composition_reference
