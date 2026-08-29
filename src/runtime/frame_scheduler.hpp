#pragma once

#include "animation/time.hpp"

#include <cstdint>
#include <optional>

namespace ryn::runtime {

struct FrameRequestCounters {
    std::uint64_t requests{0};
    std::uint64_t coalesced_requests{0};
};

class FrameRequestState final {
public:
    void request_frame() noexcept;
    [[nodiscard]] bool consume_request() noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] const FrameRequestCounters& counters() const noexcept;

private:
    bool pending_{false};
    FrameRequestCounters counters_;
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
};

} // namespace ryn::runtime
