#pragma once

#include "runtime/geometry.hpp"

#include <ryn/design_token.hpp>

#include <cstdint>
#include <variant>

namespace ryn::animation {

enum class AnimationValueKind : std::uint8_t {
    scalar,
    color,
    point,
    size,
    rect,
    logical_offset,
};

using AnimationValue = std::variant<
    float,
    Color,
    runtime::Point,
    runtime::Size,
    runtime::Rect,
    LogicalOffset>;

[[nodiscard]] AnimationValueKind value_kind(
    const AnimationValue& value) noexcept;
void validate_animation_value(const AnimationValue& value);
[[nodiscard]] AnimationValue interpolate_animation_value(
    const AnimationValue& from,
    const AnimationValue& to,
    float progress);

} // namespace ryn::animation
