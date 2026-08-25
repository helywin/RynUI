#pragma once

#include <cstdint>

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

    [[nodiscard]] virtual std::uint64_t now_milliseconds() const noexcept = 0;
    virtual bool poll_frame_event() noexcept = 0;
    virtual bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept = 0;
};

class FrameSubmitter {
public:
    virtual ~FrameSubmitter() = default;
    virtual FrameSubmissionResult submit_frame() = 0;
};

struct FrameLoopCounters {
    std::uint64_t submissions{0};
    std::uint64_t deferred_submissions{0};
    std::uint64_t failed_submissions{0};
    std::uint64_t idle_waits{0};
    std::uint64_t event_wakes{0};
    std::uint64_t last_submission_milliseconds{0};
};

class OnDemandFrameLoop final {
public:
    OnDemandFrameLoop(
        FrameRequestState& requests,
        FrameEventSource& events,
        FrameSubmitter& submitter,
        std::uint32_t idle_wait_milliseconds = 16) noexcept;

    [[nodiscard]] FrameLoopStep step();
    [[nodiscard]] const FrameLoopCounters& counters() const noexcept;

private:
    [[nodiscard]] FrameLoopStep submit_pending();

    FrameRequestState* requests_;
    FrameEventSource* events_;
    FrameSubmitter* submitter_;
    std::uint32_t idle_wait_milliseconds_;
    FrameLoopCounters counters_;
};

} // namespace ryn::runtime
