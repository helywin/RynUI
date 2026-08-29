#include "runtime/frame_scheduler.hpp"

#include <algorithm>
#include <limits>

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

std::uint64_t FrameEventSource::now_milliseconds() const noexcept {
    return static_cast<std::uint64_t>(now().count_microseconds() / 1000);
}

OnDemandFrameLoop::OnDemandFrameLoop(
    FrameRequestState& requests,
    FrameEventSource& events,
    FrameSubmitter& submitter,
    std::uint32_t idle_wait_milliseconds) noexcept
    : requests_(&requests),
      events_(&events),
      submitter_(&submitter),
      idle_wait_milliseconds_(std::max(1U, idle_wait_milliseconds)) {}

OnDemandFrameLoop::OnDemandFrameLoop(
    FrameRequestState& requests,
    FrameEventSource& events,
    FrameSubmitter& submitter,
    FrameDeadlineSource& deadlines,
    std::uint32_t idle_wait_milliseconds) noexcept
    : requests_(&requests),
      events_(&events),
      submitter_(&submitter),
      deadlines_(&deadlines),
      idle_wait_milliseconds_(std::max(1U, idle_wait_milliseconds)) {}

FrameLoopStep OnDemandFrameLoop::step() {
    auto frame_time = events_->now();
    if (events_->poll_frame_event()) {
        requests_->request_frame();
    }
    const bool initial_deadline_due = request_due_deadline(frame_time);
    if (requests_->pending()) {
        return submit_pending(frame_time, initial_deadline_due);
    }

    ++counters_.idle_waits;
    if (events_->wait_for_frame_event(wait_timeout(frame_time))) {
        ++counters_.event_wakes;
        requests_->request_frame();
    }
    frame_time = events_->now();
    const bool waited_deadline_due = request_due_deadline(frame_time);
    return requests_->pending()
        ? submit_pending(frame_time, waited_deadline_due)
        : FrameLoopStep::idle;
}

const FrameLoopCounters& OnDemandFrameLoop::counters() const noexcept {
    return counters_;
}

bool OnDemandFrameLoop::request_due_deadline(
    animation::AnimationTime now) {
    if (deadlines_ == nullptr) {
        return false;
    }
    const auto deadline = deadlines_->next_deadline();
    if (!deadline.has_value() || *deadline > now) {
        return false;
    }
    const bool already_pending = requests_->pending();
    requests_->request_frame();
    ++counters_.deadline_wakes;
    if (already_pending) {
        ++counters_.coalesced_deadline_wakes;
    }
    return true;
}

std::uint32_t OnDemandFrameLoop::wait_timeout(
    animation::AnimationTime now) const {
    if (deadlines_ == nullptr) {
        return idle_wait_milliseconds_;
    }
    const auto deadline = deadlines_->next_deadline();
    if (!deadline.has_value()) {
        return idle_wait_milliseconds_;
    }
    if (*deadline <= now) {
        return 0;
    }
    const auto remaining = *deadline - now;
    const auto microseconds = remaining.count_microseconds();
    const auto rounded_milliseconds = microseconds / 1000
        + (microseconds % 1000 == 0 ? 0 : 1);
    const auto bounded = std::min<std::uint64_t>(
        static_cast<std::uint64_t>(rounded_milliseconds),
        std::numeric_limits<std::uint32_t>::max());
    return std::min(
        idle_wait_milliseconds_, static_cast<std::uint32_t>(bounded));
}

FrameLoopStep OnDemandFrameLoop::submit_pending(
    animation::AnimationTime frame_time,
    bool deadline_due) {
    if (!requests_->consume_request()) {
        return FrameLoopStep::idle;
    }

    if (deadline_due) {
        ++counters_.animation_frames;
    }
    const bool had_animation_deadline = deadlines_ != nullptr
        && deadlines_->next_deadline().has_value();
    const auto result = submitter_->submit_frame(frame_time);
    switch (result) {
    case FrameSubmissionResult::submitted:
        ++counters_.submissions;
        counters_.last_submission_microseconds = static_cast<std::uint64_t>(
            frame_time.count_microseconds());
        counters_.last_submission_milliseconds =
            counters_.last_submission_microseconds / 1000;
        if (had_animation_deadline && deadlines_ != nullptr
                && !deadlines_->next_deadline().has_value()) {
            ++counters_.idle_after_animation;
        }
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
