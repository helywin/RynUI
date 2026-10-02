#include "runtime/frame_scheduler.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::runtime {

void FrameRequestState::request_frame() noexcept {
    ++counters_.requests;
    ++revision_;
    if (pending_) {
        ++counters_.coalesced_requests;
        animation_schedule_changed();
        return;
    }
    pending_ = true;
    animation_schedule_changed();
}

void FrameRequestState::request_invalidation_frame() noexcept {
    if (submitting_ && animation_tick_depth_) {
        ++counters_.requests;
        ++counters_.coalesced_requests;
        submission_revision_ = ++revision_;
    } else {
        request_frame();
    }
}

void FrameRequestState::bind_wake_sink(FrameWakeSink& sink) {
    if (wake_sink_ && wake_sink_ != &sink) {
        throw std::logic_error("Frame requests already have a pump");
    }
    wake_sink_ = &sink;
}

void FrameRequestState::unbind_wake_sink(FrameWakeSink& sink) noexcept {
    if (wake_sink_ == &sink) {
        wake_sink_ = nullptr;
    }
}

void FrameRequestState::animation_schedule_changed() noexcept {
    if (wake_sink_) {
        wake_sink_->wake();
    }
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

OnDemandFrameLoop::OnDemandFrameLoop(FrameRequestState& requests, FrameEventSource& events, FrameSubmitter& submitter,
                                     std::uint32_t idle_wait_milliseconds) noexcept
    : requests_(&requests), events_(&events), submitter_(&submitter),
      idle_wait_milliseconds_(std::max(1U, idle_wait_milliseconds)) {}

OnDemandFrameLoop::OnDemandFrameLoop(FrameRequestState& requests, FrameEventSource& events, FrameSubmitter& submitter,
                                     FrameDeadlineSource& deadlines, std::uint32_t idle_wait_milliseconds) noexcept
    : requests_(&requests), events_(&events), submitter_(&submitter), deadlines_(&deadlines),
      idle_wait_milliseconds_(std::max(1U, idle_wait_milliseconds)) {}

FrameLoopStep OnDemandFrameLoop::step() {
    const auto initial = tick();
    if (initial != FrameLoopStep::idle) {
        return initial;
    }
    ++counters_.idle_waits;
    if (events_->wait_for_frame_event(wait_timeout(events_->now()))) {
        ++counters_.event_wakes;
        requests_->request_frame();
    }
    return tick();
}

FrameLoopStep OnDemandFrameLoop::tick() {
    return tick(events_->now());
}

std::optional<animation::AnimationTime> OnDemandFrameLoop::next_deadline() const {
    return deadlines_ ? deadlines_->next_deadline() : std::nullopt;
}

FrameLoopStep OnDemandFrameLoop::tick(animation::AnimationTime candidate) {
    if (ticking_) {
        throw std::logic_error("Frame tick must not reenter");
    }
    ticking_ = true;

    struct Guard {
        bool& flag;

        ~Guard() {
            flag = false;
        }
    } guard{ticking_};

    const auto observation = time_cursor_.observe(candidate);
    if (observation.clamped) {
        ++counters_.clamped_timestamps;
    }
    const auto frame_time = observation.effective;
    if (events_->poll_frame_event()) {
        requests_->request_frame();
    }
    const bool initial_deadline_due = request_due_deadline(frame_time);
    if (requests_->pending()) {
        return submit_pending(frame_time, initial_deadline_due);
    }

    return FrameLoopStep::idle;
}

const FrameLoopCounters& OnDemandFrameLoop::counters() const noexcept {
    return counters_;
}

bool OnDemandFrameLoop::request_due_deadline(animation::AnimationTime now) {
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

std::uint32_t OnDemandFrameLoop::wait_timeout(animation::AnimationTime now) const {
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
    const auto rounded_milliseconds = microseconds / 1000 + (microseconds % 1000 == 0 ? 0 : 1);
    const auto bounded = std::min<std::uint64_t>(static_cast<std::uint64_t>(rounded_milliseconds),
                                                 std::numeric_limits<std::uint32_t>::max());
    return std::min(idle_wait_milliseconds_, static_cast<std::uint32_t>(bounded));
}

FrameLoopStep OnDemandFrameLoop::submit_pending(animation::AnimationTime frame_time, bool deadline_due) {
    if (!requests_->consume_request()) {
        return FrameLoopStep::idle;
    }
    pending_presentation_revision_ = requests_->revision();

    if (deadline_due) {
        ++counters_.animation_frames;
    }
    const bool had_animation_deadline = deadlines_ != nullptr && deadlines_->next_deadline().has_value();
    requests_->submitting_ = true;
    requests_->submission_revision_ = *pending_presentation_revision_;

    struct Submission {
        bool& flag;
        std::uint64_t& revision;
        std::optional<std::uint64_t>& pending;

        ~Submission() {
            if (pending) {
                pending = revision;
            }
            flag = false;
        }
    } submission{requests_->submitting_, requests_->submission_revision_, pending_presentation_revision_};

    const auto result = submitter_->submit_frame(frame_time);
    switch (result) {
    case FrameSubmissionResult::submitted:
        pending_presentation_revision_.reset();
        ++counters_.submissions;
        counters_.last_submission_microseconds = static_cast<std::uint64_t>(frame_time.count_microseconds());
        counters_.last_submission_milliseconds = counters_.last_submission_microseconds / 1000;
        if (had_animation_deadline && deadlines_ != nullptr && !deadlines_->next_deadline().has_value()) {
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
