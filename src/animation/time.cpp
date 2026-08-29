#include "animation/time.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::animation {

AnimationDuration AnimationDuration::milliseconds(double value) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            "animation duration milliseconds must be finite and non-negative");
    }
    constexpr double microseconds_per_millisecond = 1000.0;
    const double scaled = value * microseconds_per_millisecond;
    const double maximum = static_cast<double>(
        std::numeric_limits<AnimationDuration::rep>::max());
    if (!std::isfinite(scaled) || scaled > maximum) {
        throw std::overflow_error("animation duration exceeds microsecond range");
    }
    return microseconds(static_cast<rep>(std::llround(scaled)));
}

AnimationTime operator+(
    AnimationTime time,
    AnimationDuration duration) {
    const auto time_value = time.count_microseconds();
    const auto duration_value = duration.count_microseconds();
    if (duration_value
            > std::numeric_limits<AnimationTime::rep>::max() - time_value) {
        throw std::overflow_error("animation time addition overflowed");
    }
    return AnimationTime::microseconds(time_value + duration_value);
}

AnimationDuration operator-(
    AnimationTime later,
    AnimationTime earlier) {
    if (later < earlier) {
        throw std::invalid_argument(
            "animation time difference must be non-negative");
    }
    return AnimationDuration::microseconds(
        later.count_microseconds() - earlier.count_microseconds());
}

SteadyAnimationClock::SteadyAnimationClock() noexcept
    : origin_(std::chrono::steady_clock::now()) {}

AnimationTime SteadyAnimationClock::now() const noexcept {
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - origin_);
    const auto value = elapsed.count() < 0 ? 0 : elapsed.count();
    return AnimationTime::microseconds(value);
}

AnimationTime ControlledAnimationClock::now() const noexcept {
    return current_;
}

void ControlledAnimationClock::set(AnimationTime value) noexcept {
    current_ = value;
}

void ControlledAnimationClock::advance(AnimationDuration duration) {
    current_ = current_ + duration;
}

AnimationTimeObservation MonotonicTimeCursor::observe(
    AnimationTime candidate) noexcept {
    if (!last_.has_value()) {
        last_ = candidate;
        return {candidate, true, false};
    }
    if (candidate < *last_) {
        return {*last_, false, true};
    }
    if (candidate == *last_) {
        return {*last_, false, false};
    }
    last_ = candidate;
    return {candidate, true, false};
}

void MonotonicTimeCursor::reset() noexcept {
    last_.reset();
}

std::optional<AnimationTime> MonotonicTimeCursor::last() const noexcept {
    return last_;
}

AnimationIntervalSample sample_animation_interval(
    AnimationTime sample_time,
    AnimationTime start_time,
    AnimationDuration delay,
    AnimationDuration duration) {
    const auto active_start = start_time + delay;
    if (sample_time < active_start) {
        return {AnimationIntervalPhase::delayed, 0.0F};
    }
    if (duration == AnimationDuration{}) {
        return {AnimationIntervalPhase::completed, 1.0F};
    }
    const auto active_end = active_start + duration;
    if (sample_time >= active_end) {
        return {AnimationIntervalPhase::completed, 1.0F};
    }
    const auto elapsed = sample_time - active_start;
    const auto progress = static_cast<float>(
        static_cast<double>(elapsed.count_microseconds())
        / static_cast<double>(duration.count_microseconds()));
    return {AnimationIntervalPhase::active, progress};
}

} // namespace ryn::animation
