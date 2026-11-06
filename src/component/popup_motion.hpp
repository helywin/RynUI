#pragma once

#include "animation/time.hpp"
#include <algorithm>

namespace ryn::detail {
// Retained presentation only. The owner closes input immediately, even while an exit is painted.
class PopupMotion final {
public:
    void target(bool visible, animation::AnimationTime now, animation::AnimationDuration duration, bool enabled) {
        now = clock_.observe(now).effective;
        sample(now);
        const float next = visible ? 1.0F : 0.0F;
        if (!initialized_ || !enabled || duration.count_microseconds() == 0) {
            initialized_ = true;
            target_ = value_ = next;
            start_.reset();
        } else if (target_ != next) {
            target_ = next;
            from_ = value_;
            duration_ = duration;
            start_ = now;
        }
    }

    bool sample(animation::AnimationTime now) {
        now = clock_.observe(now).effective;
        if (!start_) {
            return false;
        }
        const auto elapsed = std::max<std::int64_t>(0, now.count_microseconds() - start_->count_microseconds());
        const float t = std::clamp(float(elapsed) / float(duration_.count_microseconds()), 0.0F, 1.0F);
        const float eased = 1 - (1 - t) * (1 - t) * (1 - t);
        value_ = from_ + (target_ - from_) * eased;
        if (t >= 1) {
            value_ = target_;
            start_.reset();
        }
        return true;
    }

    [[nodiscard]] bool running() const noexcept {
        return start_.has_value();
    }

    [[nodiscard]] bool painted() const noexcept {
        return target_ > 0 || running();
    }

    [[nodiscard]] float opacity() const noexcept {
        return value_;
    }

private:
    bool initialized_{};
    float value_{};
    float from_{};
    float target_{};
    animation::AnimationDuration duration_;
    std::optional<animation::AnimationTime> start_;
    animation::MonotonicTimeCursor clock_;
};
} // namespace ryn::detail
