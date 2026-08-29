#include "runtime/frame_scheduler.hpp"
#include "runtime/animation_frame_deadline.hpp"
#include "runtime/animation_frame_submitter.hpp"
#include "runtime/invalidation.hpp"

#include <deque>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ControlledEvents final : public ryn::runtime::FrameEventSource {
public:
    ryn::animation::AnimationTime now() const noexcept override {
        return current;
    }

    bool poll_frame_event() noexcept override {
        if (!poll_event) {
            return false;
        }
        poll_event = false;
        return true;
    }

    bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept override {
        if (wake_on_wait) {
            wake_on_wait = false;
            current = current
                + ryn::animation::AnimationDuration::microseconds(1000);
            return true;
        }
        current = current + ryn::animation::AnimationDuration::microseconds(
            static_cast<std::int64_t>(timeout_milliseconds) * 1000);
        last_timeout = timeout_milliseconds;
        return false;
    }

    ryn::animation::AnimationTime current;
    std::uint32_t last_timeout{0};
    bool poll_event{false};
    bool wake_on_wait{false};
};

class ControlledSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        ++calls;
        timestamps.push_back(frame_time);
        if (results.empty()) {
            return ryn::runtime::FrameSubmissionResult::submitted;
        }
        const auto result = results.front();
        results.pop_front();
        return result;
    }

    std::deque<ryn::runtime::FrameSubmissionResult> results;
    std::vector<ryn::animation::AnimationTime> timestamps;
    int calls{0};
};

class ControlledDeadline final : public ryn::runtime::FrameDeadlineSource {
public:
    [[nodiscard]] std::optional<ryn::animation::AnimationTime>
    next_deadline() const override {
        return deadline;
    }

    std::optional<ryn::animation::AnimationTime> deadline;
};

class RecordingAnimationSink final : public ryn::animation::AnimationTargetSink {
public:
    void apply(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId,
        const ryn::animation::AnimationValue& value,
        ryn::animation::AnimationDirtyDomain) override {
        values.push_back(std::get<float>(value));
        dirty = true;
    }

    void completed(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId) override {
        ++completions;
    }

    std::vector<float> values;
    int completions{0};
    bool dirty{false};
};

class DeferredDownstream final : public ryn::runtime::FrameSubmitter {
public:
    explicit DeferredDownstream(RecordingAnimationSink& sink) noexcept
        : sink_(&sink) {}

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        timestamps.push_back(frame_time);
        if (!results.empty()) {
            const auto result = results.front();
            results.pop_front();
            if (result == ryn::runtime::FrameSubmissionResult::submitted) {
                sink_->dirty = false;
            }
            return result;
        }
        sink_->dirty = false;
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

    RecordingAnimationSink* sink_;
    std::deque<ryn::runtime::FrameSubmissionResult> results;
    std::vector<ryn::animation::AnimationTime> timestamps;
};

void test_requests_coalesce_and_idle_does_not_submit() {
    ryn::runtime::FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    ryn::runtime::OnDemandFrameLoop loop(requests, events, submitter, 16);

    requests.request_frame();
    requests.request_frame();
    requests.request_frame();
    require(requests.counters().requests == 3
                && requests.counters().coalesced_requests == 2,
            "frame requests were not coalesced");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "initial frame request was not submitted");
    require(submitter.calls == 1, "coalesced requests submitted more than one frame");

    for (int refresh_tick = 0; refresh_tick < 120; ++refresh_tick) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "stable frame loop left the idle state");
    }
    require(submitter.calls == 1, "idle loop submitted at refresh rate");
    require(events.now_milliseconds() == 120U * 16U,
            "controlled clock did not advance through idle waits");
    require(loop.counters().idle_waits == 120, "idle wait counter is incorrect");
}

void test_deadline_wait_rounding_and_event_coalescing() {
    using namespace ryn::animation;
    ryn::runtime::FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    ControlledDeadline deadlines;
    deadlines.deadline = AnimationTime::microseconds(500);
    ryn::runtime::OnDemandFrameLoop loop(
        requests, events, submitter, deadlines, 16);

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "sub-millisecond deadline did not wake a frame");
    require(events.last_timeout == 1
                && submitter.timestamps.back() == AnimationTime::microseconds(1000),
            "deadline wait was not rounded up to a bounded millisecond wait");
    require(loop.counters().deadline_wakes == 1
                && loop.counters().animation_frames == 1,
            "deadline wake diagnostics are incorrect");

    events.poll_event = true;
    deadlines.deadline = events.now();
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "coincident input and deadline did not submit");
    require(loop.counters().coalesced_deadline_wakes == 1
                && submitter.calls == 2,
            "coincident input and deadline were not coalesced");
}

void test_animation_pipeline_deferred_retry_and_idle_recovery() {
    using namespace ryn::animation;
    AnimationRuntime animations;
    animations.reserve(2, 1, 1);
    animations.set_nominal_frame_period(AnimationDuration::microseconds(1000));
    RecordingAnimationSink sink;
    const auto scope = animations.create_scope();
    const auto target = animations.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material | AnimationDirtyDomain::animation);
    static_cast<void>(animations.play(
        target,
        0.0F,
        1.0F,
        {{}, AnimationDuration::microseconds(3000), Easing::linear()},
        {}));

    ryn::runtime::FrameRequestState requests;
    ControlledEvents events;
    DeferredDownstream downstream(sink);
    downstream.results.push_back(
        ryn::runtime::FrameSubmissionResult::deferred);
    downstream.results.push_back(
        ryn::runtime::FrameSubmissionResult::submitted);
    ryn::runtime::AnimationFrameSubmitter submitter(animations, downstream);
    ryn::runtime::AnimationFrameDeadlineSource deadlines(animations);
    ryn::runtime::OnDemandFrameLoop loop(
        requests, events, submitter, deadlines, 10);

    require(loop.step() == ryn::runtime::FrameLoopStep::deferred
                && sink.dirty && sink.values.size() == 2,
            "deferred animation frame lost dirty state or sampled incorrectly");
    events.poll_event = true;
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted
                && !sink.dirty && sink.values.size() == 2,
            "same-timestamp retry advanced animation or failed to clear dirty state");
    require(downstream.timestamps[0] == downstream.timestamps[1],
            "same-timestamp deferred retry did not preserve frame time");

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "second animation deadline did not submit");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "final animation deadline did not submit");
    require(sink.values.back() == 1.0F && sink.completions == 1
                && animations.size() == 0,
            "animation pipeline did not reach its exact final state");
    const auto calls_after_completion = downstream.timestamps.size();
    for (int idle = 0; idle < 10; ++idle) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "completed animation did not restore frame-loop idle");
    }
    require(downstream.timestamps.size() == calls_after_completion
                && loop.counters().idle_after_animation == 1
                && submitter.counters().animation_updates == 3,
            "idle recovery or animation pipeline diagnostics are incorrect");
}

void test_event_wakes_idle_and_deferred_frame_does_not_spin() {
    ryn::runtime::FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    ryn::runtime::OnDemandFrameLoop loop(requests, events, submitter, 10);

    events.wake_on_wait = true;
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "event wait did not wake and submit a frame");
    require(loop.counters().event_wakes == 1 && submitter.calls == 1,
            "event wake counters are incorrect");

    submitter.results.push_back(ryn::runtime::FrameSubmissionResult::deferred);
    requests.request_frame();
    require(loop.step() == ryn::runtime::FrameLoopStep::deferred,
            "deferred swapchain frame was not reported");
    require(!requests.pending(), "deferred frame remained continuously requested");

    for (int idle_step = 0; idle_step < 60; ++idle_step) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "deferred frame did not settle to idle");
    }
    require(submitter.calls == 2, "deferred frame caused continuous resubmission");

    events.poll_event = true;
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "subsequent input event did not wake rendering");
    require(submitter.calls == 3, "input wake did not submit exactly one frame");
}

void test_dirty_update_requests_a_frame() {
    ryn::runtime::NodeStore nodes;
    const auto node = nodes.create_root();
    ryn::runtime::FrameRequestState requests;
    ryn::runtime::DirtyQueues dirty(nodes, &requests);
    ryn::runtime::NodePropertyWriter properties(nodes, dirty);

    require(properties.set_color(node, {0.2F, 0.5F, 0.9F, 1.0F}),
            "dirty property update was suppressed");
    require(requests.pending(), "dirty property update did not request a frame");
    require(!properties.set_color(node, {0.2F, 0.5F, 0.9F, 1.0F}),
            "equal property update was not suppressed");
    require(requests.counters().requests == 1,
            "equal property update requested an extra frame");
}

} // namespace

int main() {
    try {
        test_requests_coalesce_and_idle_does_not_submit();
        test_event_wakes_idle_and_deferred_frame_does_not_spin();
        test_deadline_wait_rounding_and_event_coalescing();
        test_animation_pipeline_deferred_retry_and_idle_recovery();
        test_dirty_update_requests_a_frame();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
