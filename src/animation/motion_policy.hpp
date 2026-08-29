#pragma once

#include "animation/runtime.hpp"

#include <ryn/theme.hpp>

#include <array>
#include <cstdint>

namespace ryn::animation {

enum class MotionPreference : std::uint8_t {
    normal,
    reduced,
};

enum class MotionDurationToken : std::uint8_t {
    fast,
    mid,
    slow,
};

enum class MotionEasingToken : std::uint8_t {
    ease_out_circ,
    ease_in_out_circ,
    ease_out,
    ease_in_out,
    ease_out_back,
    ease_in_back,
    ease_in_quint,
    ease_out_quint,
};

struct MotionTokenSet final {
    AnimationDuration unit;
    AnimationDuration base;
    AnimationDuration fast;
    AnimationDuration mid;
    AnimationDuration slow;
    std::array<Easing, 8> easings;
    bool theme_motion_enabled{true};

    [[nodiscard]] AnimationDuration duration(
        MotionDurationToken token) const;
    [[nodiscard]] Easing easing(MotionEasingToken token) const;

    friend constexpr bool operator==(
        const MotionTokenSet&,
        const MotionTokenSet&) = default;
};

[[nodiscard]] MotionTokenSet resolve_motion_tokens(
    const ThemeSnapshot& theme);

class MotionPolicy final {
public:
    MotionPolicy(
        MotionTokenSet tokens,
        MotionPreference preference = MotionPreference::normal) noexcept;

    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] bool reduced() const noexcept;
    [[nodiscard]] MotionPreference preference() const noexcept;
    [[nodiscard]] const MotionTokenSet& tokens() const noexcept;
    [[nodiscard]] AnimationSpec transition(
        MotionDurationToken duration,
        MotionEasingToken easing,
        AnimationDuration delay = {}) const;

    friend constexpr bool operator==(const MotionPolicy&, const MotionPolicy&) = default;

private:
    MotionTokenSet tokens_;
    MotionPreference preference_{MotionPreference::normal};
};

[[nodiscard]] MotionPolicy resolve_motion_policy(
    const ThemeSnapshot& theme,
    MotionPreference preference = MotionPreference::normal);

class MotionPolicyController final {
public:
    MotionPolicyController(
        AnimationRuntime& runtime,
        MotionPolicy policy) noexcept;

    bool update(MotionPolicy policy);
    [[nodiscard]] const MotionPolicy& policy() const noexcept;

private:
    AnimationRuntime* runtime_;
    MotionPolicy policy_;
};

} // namespace ryn::animation
