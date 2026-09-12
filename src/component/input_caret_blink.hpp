#pragma once

#include "animation/time.hpp"
#include <algorithm>
#include <limits>

namespace ryn::detail {
// A retained, discrete deadline, independent of the display refresh cadence.
class InputCaretBlink final {
public:
    static constexpr std::int64_t period_us = 500000;
    bool configure(bool eligible, bool animated, animation::AnimationTime now, bool reset = false) {
        now = observe(now);
        if(!reset && eligible_ == eligible && animated_ == animated) return false;
        eligible_ = eligible; animated_ = animated;
        const bool changed = visible_ != eligible;
        visible_ = eligible;
        deadline_ = eligible && animated ? after(now) : std::nullopt;
        return changed;
    }
    bool tick(animation::AnimationTime now) {
        now = observe(now);
        if(!deadline_ || now < *deadline_) return false;
        const auto steps = (now - *deadline_).count_microseconds() / period_us + 1;
        const auto remainder = (now - *deadline_).count_microseconds() % period_us;
        const auto next_delay = period_us - remainder;
        const bool previous = visible_;
        if(steps % 2) visible_ = !visible_;
        deadline_ = after(now, next_delay);
        if(!deadline_) visible_ = eligible_; // Clock exhaustion remains static/idle.
        return previous != visible_;
    }
    void stop() noexcept { eligible_ = false; visible_ = false; deadline_.reset(); }
    bool override_deadline(std::optional<animation::AnimationTime> next) noexcept {
        if(next == deadline_) return false;
        deadline_ = next; return true;
    }
    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] std::optional<animation::AnimationTime> deadline() const noexcept { return deadline_; }
private:
    animation::AnimationTime observe(animation::AnimationTime now) noexcept {
        last_ = std::max(last_, now); return last_;
    }
    static std::optional<animation::AnimationTime> after(animation::AnimationTime now, std::int64_t delay = period_us) {
        if(now.count_microseconds() > std::numeric_limits<std::int64_t>::max() - delay) return {};
        return now + animation::AnimationDuration::microseconds(delay);
    }
    bool eligible_{}, animated_{}, visible_{};
    animation::AnimationTime last_;
    std::optional<animation::AnimationTime> deadline_;
};
} // namespace ryn::detail
