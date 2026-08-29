#include "animation/motion_policy.hpp"

#include <stdexcept>
#include <utility>

namespace ryn::animation {
namespace {

AnimationDuration duration_from_theme(
    Duration base,
    Duration unit,
    double multiplier) {
    return AnimationDuration::milliseconds(
        static_cast<double>(base.count_milliseconds())
        + static_cast<double>(unit.count_milliseconds()) * multiplier);
}

} // namespace

AnimationDuration MotionTokenSet::duration(
    MotionDurationToken token) const {
    switch (token) {
    case MotionDurationToken::fast:
        return fast;
    case MotionDurationToken::mid:
        return mid;
    case MotionDurationToken::slow:
        return slow;
    }
    throw std::invalid_argument("motion duration token is invalid");
}

Easing MotionTokenSet::easing(MotionEasingToken token) const {
    const auto index = static_cast<std::size_t>(token);
    if (index >= easings.size()) {
        throw std::invalid_argument("motion easing token is invalid");
    }
    return easings[index];
}

MotionTokenSet resolve_motion_tokens(const ThemeSnapshot& theme) {
    const auto& map = theme.map();
    const auto unit = AnimationDuration::milliseconds(
        map.motion_unit.count_milliseconds());
    const auto base = AnimationDuration::milliseconds(
        map.motion_base.count_milliseconds());
    return {
        unit,
        base,
        duration_from_theme(map.motion_base, map.motion_unit, 1.0),
        duration_from_theme(map.motion_base, map.motion_unit, 2.0),
        duration_from_theme(map.motion_base, map.motion_unit, 3.0),
        {
            ant_easing(AntEasingPreset::ease_out_circ),
            ant_easing(AntEasingPreset::ease_in_out_circ),
            ant_easing(AntEasingPreset::ease_out),
            ant_easing(AntEasingPreset::ease_in_out),
            ant_easing(AntEasingPreset::ease_out_back),
            ant_easing(AntEasingPreset::ease_in_back),
            ant_easing(AntEasingPreset::ease_in_quint),
            ant_easing(AntEasingPreset::ease_out_quint),
        },
        map.motion,
    };
}

MotionPolicy::MotionPolicy(
    MotionTokenSet tokens,
    MotionPreference preference) noexcept
    : tokens_(std::move(tokens)), preference_(preference) {}

bool MotionPolicy::enabled() const noexcept {
    return tokens_.theme_motion_enabled
        && preference_ == MotionPreference::normal;
}

bool MotionPolicy::reduced() const noexcept {
    return preference_ == MotionPreference::reduced;
}

MotionPreference MotionPolicy::preference() const noexcept {
    return preference_;
}

const MotionTokenSet& MotionPolicy::tokens() const noexcept {
    return tokens_;
}

AnimationSpec MotionPolicy::transition(
    MotionDurationToken duration_token,
    MotionEasingToken easing_token,
    AnimationDuration delay) const {
    const auto easing_value = tokens_.easing(easing_token);
    return enabled()
        ? AnimationSpec{delay, tokens_.duration(duration_token), easing_value}
        : AnimationSpec{{}, {}, easing_value};
}

MotionPolicy resolve_motion_policy(
    const ThemeSnapshot& theme,
    MotionPreference preference) {
    return MotionPolicy(resolve_motion_tokens(theme), preference);
}

MotionPolicyController::MotionPolicyController(
    AnimationRuntime& runtime,
    MotionPolicy policy) noexcept
    : runtime_(&runtime), policy_(std::move(policy)) {}

bool MotionPolicyController::update(MotionPolicy policy) {
    if (policy_ == policy) {
        return false;
    }
    policy_ = std::move(policy);
    if (!policy_.enabled()) {
        static_cast<void>(runtime_->finish_all());
    }
    return true;
}

const MotionPolicy& MotionPolicyController::policy() const noexcept {
    return policy_;
}

} // namespace ryn::animation
