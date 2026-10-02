#pragma once

namespace ryn::animation {
// Owner-thread notification; observers must not execute a frame inline.
class AnimationScheduleObserver {
public:
    virtual ~AnimationScheduleObserver() = default;
    virtual void animation_schedule_changed() noexcept = 0;

    virtual void animation_tick_started() noexcept {}

    virtual void animation_tick_finished() noexcept {}
};
} // namespace ryn::animation
