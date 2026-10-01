#pragma once

#include "animation/time.hpp"
#include "animation/schedule_observer.hpp"

#include <cstdint>
#include <optional>

namespace ryn::runtime {

struct FrameRequestCounters {
    std::uint64_t requests{0};
    std::uint64_t coalesced_requests{0};
};

class FrameWakeSink {
public:
    virtual ~FrameWakeSink() = default;
    virtual void wake() noexcept = 0;
};

class FrameRequestState final : public animation::AnimationScheduleObserver {
public:
    void request_frame() noexcept;
    // Dirty work sampled before this frame's layout/uploads can be consumed
    // by this frame. Explicit request_frame always requests the next epoch.
    void request_invalidation_frame() noexcept;
    [[nodiscard]] bool consume_request() noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] const FrameRequestCounters& counters() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    void bind_wake_sink(FrameWakeSink& sink);
    void unbind_wake_sink(FrameWakeSink& sink) noexcept;
    void animation_schedule_changed() noexcept override;
    void animation_tick_started() noexcept override { ++animation_tick_depth_; }
    void animation_tick_finished() noexcept override { --animation_tick_depth_; }

private:
    friend class OnDemandFrameLoop;
    bool pending_{false};
    FrameRequestCounters counters_;
    std::uint64_t revision_{};
    FrameWakeSink* wake_sink_{};
    unsigned animation_tick_depth_{};
    bool submitting_{};
    std::uint64_t submission_revision_{};
};

enum class FrameSubmissionResult {
    submitted,
    deferred,
    failed,
};

enum class FrameLoopStep {
    submitted,
    deferred,
    idle,
    failed,
};

class FrameEventSource {
public:
    virtual ~FrameEventSource() = default;

    [[nodiscard]] virtual animation::AnimationTime now() const noexcept = 0;
    [[nodiscard]] std::uint64_t now_milliseconds() const noexcept;
    virtual bool poll_frame_event() noexcept = 0;
    virtual bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept = 0;
};

class FrameDeadlineSource {
public:
    virtual ~FrameDeadlineSource() = default;
    [[nodiscard]] virtual std::optional<animation::AnimationTime>
        next_deadline() const = 0;
};

class FrameSubmitter {
public:
    virtual ~FrameSubmitter() = default;
    virtual FrameSubmissionResult submit_frame(
        animation::AnimationTime frame_time) = 0;
};

struct FrameLoopCounters {
    std::uint64_t submissions{0};
    std::uint64_t deferred_submissions{0};
    std::uint64_t failed_submissions{0};
    std::uint64_t idle_waits{0};
    std::uint64_t event_wakes{0};
    std::uint64_t deadline_wakes{0};
    std::uint64_t coalesced_deadline_wakes{0};
    std::uint64_t animation_frames{0};
    std::uint64_t idle_after_animation{0};
    std::uint64_t last_submission_milliseconds{0};
    std::uint64_t last_submission_microseconds{0};
    std::uint64_t clamped_timestamps{0};
};

class OnDemandFrameLoop final {
public:
    OnDemandFrameLoop(
        FrameRequestState& requests,
        FrameEventSource& events,
        FrameSubmitter& submitter,
        std::uint32_t idle_wait_milliseconds = 16) noexcept;
    OnDemandFrameLoop(
        FrameRequestState& requests,
        FrameEventSource& events,
        FrameSubmitter& submitter,
        FrameDeadlineSource& deadlines,
        std::uint32_t idle_wait_milliseconds = 16) noexcept;

    [[nodiscard]] FrameLoopStep step();
    // Poll and submit at most once; never waits. Callback timestamps use the
    // same monotonic microsecond clock domain as FrameEventSource.
    [[nodiscard]] FrameLoopStep tick();
    [[nodiscard]] FrameLoopStep tick(animation::AnimationTime frame_time);
    [[nodiscard]] std::optional<animation::AnimationTime> next_deadline() const;
    [[nodiscard]] std::optional<std::uint64_t> pending_presentation_revision() const noexcept {
        return pending_presentation_revision_;
    }
    [[nodiscard]] const FrameLoopCounters& counters() const noexcept;

private:
    [[nodiscard]] bool request_due_deadline(
        animation::AnimationTime now);
    [[nodiscard]] std::uint32_t wait_timeout(
        animation::AnimationTime now) const;
    [[nodiscard]] FrameLoopStep submit_pending(
        animation::AnimationTime frame_time,
        bool deadline_due);

    FrameRequestState* requests_;
    FrameEventSource* events_;
    FrameSubmitter* submitter_;
    FrameDeadlineSource* deadlines_{nullptr};
    std::uint32_t idle_wait_milliseconds_;
    FrameLoopCounters counters_;
    animation::MonotonicTimeCursor time_cursor_;
    std::optional<std::uint64_t> pending_presentation_revision_;
    bool ticking_{false};
};

} // namespace ryn::runtime
