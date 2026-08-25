#include "runtime/frame_scheduler.hpp"

namespace ryn::runtime {

void FrameRequestState::request_frame() noexcept {
    ++counters_.requests;
    if (pending_) {
        ++counters_.coalesced_requests;
        return;
    }
    pending_ = true;
}

bool FrameRequestState::consume_request() noexcept {
    if (!pending_) {
        return false;
    }
    pending_ = false;
    return true;
}

bool FrameRequestState::pending() const noexcept {
    return pending_;
}

const FrameRequestCounters& FrameRequestState::counters() const noexcept {
    return counters_;
}

OnDemandFrameLoop::OnDemandFrameLoop(
    FrameRequestState& requests,
    FrameEventSource& events,
    FrameSubmitter& submitter,
    std::uint32_t idle_wait_milliseconds) noexcept
    : requests_(&requests),
      events_(&events),
      submitter_(&submitter),
      idle_wait_milliseconds_(idle_wait_milliseconds) {}

FrameLoopStep OnDemandFrameLoop::step() {
    if (events_->poll_frame_event()) {
        requests_->request_frame();
    }
    if (requests_->pending()) {
        return submit_pending();
    }

    ++counters_.idle_waits;
    if (!events_->wait_for_frame_event(idle_wait_milliseconds_)) {
        return FrameLoopStep::idle;
    }
    ++counters_.event_wakes;
    requests_->request_frame();
    return submit_pending();
}

const FrameLoopCounters& OnDemandFrameLoop::counters() const noexcept {
    return counters_;
}

FrameLoopStep OnDemandFrameLoop::submit_pending() {
    if (!requests_->consume_request()) {
        return FrameLoopStep::idle;
    }

    const auto result = submitter_->submit_frame();
    switch (result) {
    case FrameSubmissionResult::submitted:
        ++counters_.submissions;
        counters_.last_submission_milliseconds = events_->now_milliseconds();
        return FrameLoopStep::submitted;
    case FrameSubmissionResult::deferred:
        ++counters_.deferred_submissions;
        return FrameLoopStep::deferred;
    case FrameSubmissionResult::failed:
        ++counters_.failed_submissions;
        return FrameLoopStep::failed;
    }
    ++counters_.failed_submissions;
    return FrameLoopStep::failed;
}

} // namespace ryn::runtime
