#pragma once

#include <ryn/design_token.hpp>

#include <cstdint>

namespace ryn::animation {

enum class EasingKind : std::uint8_t {
    linear,
    cubic_bezier,
};

class Easing final {
public:
    [[nodiscard]] static constexpr Easing linear() noexcept {
        return Easing(EasingKind::linear, CubicBezier{0.0F, 0.0F, 1.0F, 1.0F});
    }

    [[nodiscard]] static constexpr Easing cubic_bezier(CubicBezier curve) noexcept {
        return Easing(EasingKind::cubic_bezier, curve);
    }

    [[nodiscard]] constexpr EasingKind kind() const noexcept { return kind_; }
    [[nodiscard]] constexpr CubicBezier curve() const noexcept { return curve_; }
    [[nodiscard]] float sample(float normalized_time) const;

    friend constexpr bool operator==(Easing, Easing) = default;

private:
    constexpr Easing(EasingKind kind, CubicBezier curve) noexcept
        : kind_(kind), curve_(curve) {}

    EasingKind kind_{EasingKind::linear};
    CubicBezier curve_;
};

enum class AntEasingPreset : std::uint8_t {
    ease_out_circ,
    ease_in_out_circ,
    ease_out,
    ease_in_out,
    ease_out_back,
    ease_in_back,
    ease_in_quint,
    ease_out_quint,
};

[[nodiscard]] Easing ant_easing(AntEasingPreset preset);

} // namespace ryn::animation
