#pragma once

#include "runtime/geometry.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::detail {

enum class LockedShapeKind {
    fill,
    outline,
    hard_shadow,
    exterior_shadow,
};

struct LockedShapeGeometry final {
    runtime::Rect bounds;
    float radius{};
    LockedShapeKind kind{};
    float outline_width{};
    float outline_offset{};
    runtime::Point shadow_offset;
    float shadow_blur{};
    float shadow_spread{};
    float shadow_caster_inset{};
    float shadow_alpha{1};
    bool dark_ink{};
};

struct LockedShapeCoverage final {
    std::uint32_t left{};
    std::uint32_t top{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> pixels;
};

// Inputs and outputs are physical pixels. Ancestor clipping belongs to the
// consuming draw and does not trim the geometry that casts an exterior shadow.
[[nodiscard]] std::optional<LockedShapeCoverage>
rasterize_locked_shape(const LockedShapeGeometry& shape, std::uint32_t viewport_width, std::uint32_t viewport_height);

} // namespace ryn::detail
