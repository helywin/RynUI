#pragma once
#include "runtime/geometry.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::runtime {
// Window logical coordinates. This is geometry, never a primitive hard clip.
struct RoundedContentClip final {
    Rect bounds;
    float radius{};
    friend bool operator==(const RoundedContentClip&, const RoundedContentClip&) = default;
};

inline RoundedContentClip normalized_content_clip(RoundedContentClip clip) {
    const auto& r = clip.bounds;
    if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.width) || !std::isfinite(r.height) ||
        !std::isfinite(clip.radius) || r.width < 0 || r.height < 0 || clip.radius < 0 ||
        !std::isfinite(double(r.x) + r.width) || !std::isfinite(double(r.y) + r.height)) {
        throw std::invalid_argument("Rounded content clip must be finite and nonnegative");
    }
    clip.radius = std::min(clip.radius, std::min(r.width, r.height) * .5F);
    return clip;
}

inline bool content_clip_contains(const RoundedContentClip& clip, Point p) noexcept {
    const auto& r = clip.bounds;
    if (p.x < r.x || p.y < r.y || double(p.x) >= double(r.x) + r.width || double(p.y) >= double(r.y) + r.height ||
        !std::isfinite(p.x) || !std::isfinite(p.y)) {
        return false;
    }
    const double radius = std::min(double(clip.radius), std::min(double(r.width), double(r.height)) * .5);
    const double cx = std::clamp(double(p.x), double(r.x) + radius, double(r.x) + r.width - radius);
    const double cy = std::clamp(double(p.y), double(r.y) + radius, double(r.y) + r.height - radius);
    const double dx = double(p.x) - cx;
    const double dy = double(p.y) - cy;
    return dx * dx + dy * dy <= radius * radius;
}
} // namespace ryn::runtime
