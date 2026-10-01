#include "runtime/frame_scheduler.hpp"
#include "runtime/animation_frame_deadline.hpp"
#include "runtime/animation_frame_submitter.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/callback_frame_pump.hpp"

#include <functional>
#include <thread>

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
        ++waits;
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
    int waits{};
};

class ControlledSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        ++calls;
        timestamps.push_back(frame_time);
        if (on_submit) on_submit();
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
    std::function<void()> on_submit;
};

class ControlledDeadline final : public ryn::runtime::FrameDeadlineSource {
public:
    [[nodiscard]] std::optional<ryn::animation::AnimationTime>
    next_deadline() const override {
        return deadline;
    }

    std::optional<ryn::animation::AnimationTime> deadline;
};

class CallbackHost final : public ryn::runtime::FrameCallbackHost {
public:
    void replace_callback(std::optional<ryn::animation::AnimationTime> time,
        ryn::runtime::FrameCallback callback) noexcept override {
        deadline = time;
        current = callback;
        ++replacements;
        scheduled = true;
    }
    void cancel_callback() noexcept override { ++cancellations; scheduled = false; }
    ryn::runtime::FrameLoopStep fire(ryn::animation::AnimationTime time) {
        scheduled = false;
        return current.run(time);
    }
    ryn::runtime::FrameCallback current;
    std::optional<ryn::animation::AnimationTime> deadline;
    bool scheduled{};
    int replacements{}, cancellations{};
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
        if (on_apply) on_apply();
    }

    void completed(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId) override {
        ++completions;
        if (on_complete) on_complete();
    }

    std::vector<float> values;
    int completions{0};
    bool dirty{false};
    std::function<void()> on_apply, on_complete;
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

void test_nonblocking_callback_requests_and_lifetime() {
    using namespace ryn::runtime;
    using ryn::animation::AnimationTime;
    FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    OnDemandFrameLoop loop(requests, events, submitter);
    CallbackHost host;
    FrameCallback retired;
    {
        CallbackFramePump pump(requests, loop, host);
        require(!host.scheduled, "idle pump scheduled callback");
        NodeStore nodes;
        const auto node = nodes.create_root();
        DirtyQueues dirty(nodes, &requests);
        NodePropertyWriter properties(nodes, dirty);
        require(properties.set_color(node, {0.2F, 0.5F, 0.9F, 1}), "property update failed");
        requests.request_frame();
        requests.request_frame();
        require(host.replacements == 1 && host.scheduled && !host.deadline,
            "Core property requests did not coalesce into immediate callback");
        const auto previous = host.current;
        submitter.on_submit = [&] {
            require(previous.run(AnimationTime::microseconds(100)) == FrameLoopStep::idle,
                "callback reentered frame submission");
        };
        require(host.fire(AnimationTime::microseconds(100)) == FrameLoopStep::submitted,
            "callback did not submit");
        require(events.waits == 0 && submitter.calls == 1 && !host.scheduled,
            "callback waited or idle callback remained scheduled");
        submitter.on_submit = {};
        requests.request_frame();
        require(previous.run(AnimationTime::microseconds(200)) == FrameLoopStep::idle,
            "old callback submitted newer request");
        require(host.fire(AnimationTime::microseconds(50)) == FrameLoopStep::submitted
            && submitter.timestamps.back() == AnimationTime::microseconds(100)
            && loop.counters().clamped_timestamps == 1, "callback clock moved backwards");
        requests.request_frame();
        retired = host.current;
        auto wrong_thread_result = FrameLoopStep::submitted;
        std::thread foreign([&] { wrong_thread_result = retired.run(AnimationTime::microseconds(300)); });
        foreign.join();
        require(wrong_thread_result == FrameLoopStep::idle, "foreign-thread callback accepted");
    }
    require(retired.run(AnimationTime::microseconds(400)) == FrameLoopStep::idle
        && host.cancellations == 1, "destroyed pump callback touched released object");
    require(loop.tick() == FrameLoopStep::submitted && events.waits == 0,
        "nonblocking tick did not remain usable after callback pump destruction");
}

void test_callback_animation_schedule_changes() {
    using namespace ryn::animation;
    using namespace ryn::runtime;
    FrameRequestState requests;
    AnimationRuntime animations;
    animations.set_schedule_observer(&requests);
    animations.set_nominal_frame_period(AnimationDuration::microseconds(1000));
    RecordingAnimationSink sink;
    const auto scope = animations.create_scope();
    const auto target = animations.register_target(scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material);
    ControlledEvents events;
    ControlledSubmitter downstream;
    AnimationFrameSubmitter submitter(animations, downstream);
    AnimationFrameDeadlineSource deadlines(animations);
    OnDemandFrameLoop loop(requests, events, submitter, deadlines);
    CallbackHost host;
    CallbackFramePump pump(requests, loop, host);
    const auto id = animations.play(target, 0.0F, 1.0F,
        {AnimationDuration::microseconds(10000), AnimationDuration::microseconds(3000), Easing::linear()}, {});
    require(host.scheduled && host.deadline == AnimationTime::microseconds(10000),
        "animation play did not schedule future callback");
    const auto late = host.current;
    require(animations.retarget(id, 1.0F,
        {AnimationDuration::microseconds(2000), AnimationDuration::microseconds(3000), Easing::linear()}, {}),
        "animation retarget failed");
    require(host.replacements == 2 && host.deadline == AnimationTime::microseconds(2000),
        "earlier animation deadline did not replace callback");
    require(late.run(AnimationTime::microseconds(10000)) == FrameLoopStep::idle,
        "replaced deadline callback submitted");
    const auto canceled = host.current;
    require(animations.cancel(id, {}) && !host.scheduled && host.cancellations == 1,
        "animation cancellation left deadline scheduled");
    require(canceled.run(AnimationTime::microseconds(2000)) == FrameLoopStep::idle,
        "canceled deadline callback submitted");
    static_cast<void>(animations.play(target, 0.0F, 1.0F,
        {{}, AnimationDuration::microseconds(2000), Easing::linear()}, {}));
    require(host.fire(AnimationTime::microseconds(1000)) == FrameLoopStep::submitted
        && host.deadline == AnimationTime::microseconds(2000), "animation callback did not advance deadline");
    require(host.fire(AnimationTime::microseconds(2000)) == FrameLoopStep::submitted
        && !host.scheduled && animations.size() == 0 && events.waits == 0,
        "completed animation did not restore callback idle");
    sink.on_apply = [&] { requests.request_invalidation_frame(); };
    sink.on_complete = [&] { requests.request_frame(); };
    static_cast<void>(animations.play(target, 0.0F, 1.0F,
        {{}, AnimationDuration::microseconds(1000), Easing::linear()}, AnimationTime::microseconds(2000)));
    downstream.results.push_back(FrameSubmissionResult::deferred);
    require(host.fire(AnimationTime::microseconds(3000)) == FrameLoopStep::deferred
        && loop.pending_presentation_revision() && *loop.pending_presentation_revision() >= 2
        && requests.pending() && host.scheduled && !host.deadline,
        "completion explicit request swallowed or sampled deferred revision lost");
    require(host.fire(AnimationTime::microseconds(3000)) == FrameLoopStep::submitted
        && !requests.pending() && !host.scheduled, "completion next epoch did not settle idle");
}

void test_pump_destroyed_during_submission() {
    using namespace ryn::runtime;
    FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    OnDemandFrameLoop loop(requests, events, submitter);
    CallbackHost host;
    auto pump = std::make_unique<CallbackFramePump>(requests, loop, host);
    requests.request_frame();
    const auto callback = host.current;
    submitter.on_submit = [&] { pump.reset(); };
    require(host.fire({}) == FrameLoopStep::submitted && !pump && !host.scheduled,
        "pump destruction inside submission failed");
    require(callback.run({}) == FrameLoopStep::idle, "callback survived active pump destruction");
    pump = std::make_unique<CallbackFramePump>(requests, loop, host);
    requests.request_frame();
    submitter.on_submit = [&] { pump.reset(); throw std::runtime_error("after destruction"); };
    bool caught = false;
    try { static_cast<void>(host.fire({})); } catch (const std::runtime_error&) { caught = true; }
    require(caught && !pump && !host.scheduled, "exception after pump destruction accessed dead pump");
    submitter.on_submit = {};
    pump = std::make_unique<CallbackFramePump>(requests, loop, host);
    requests.request_frame();
    submitter.on_submit = [&] { requests.request_frame(); };
    require(host.fire({}) == FrameLoopStep::submitted && requests.pending() && host.scheduled,
        "late non-animation request was consumed by current frame");
}

void test_deferred_revision_and_reentrant_tick() {
    using namespace ryn::runtime;
    FrameRequestState requests;
    ControlledEvents events;
    ControlledSubmitter submitter;
    OnDemandFrameLoop loop(requests, events, submitter);
    requests.request_frame();
    submitter.results.push_back(FrameSubmissionResult::deferred);
    require(loop.tick() == FrameLoopStep::deferred && loop.pending_presentation_revision() == 1
        && !requests.pending(), "deferred revision became immediate wake or disappeared");
    for (int i = 0; i < 100; ++i) require(loop.tick() == FrameLoopStep::idle, "deferred busy loop");
    requests.request_frame();
    submitter.results.push_back(FrameSubmissionResult::deferred);
    require(loop.tick() == FrameLoopStep::deferred && loop.pending_presentation_revision() == 2,
        "deferred newer revision did not supersede previous content");
    bool rejected = false;
    submitter.on_submit = [&] { try { static_cast<void>(loop.tick()); }
        catch (const std::logic_error&) { rejected = true; } };
    events.poll_event = true;
    require(loop.tick() == FrameLoopStep::submitted && !loop.pending_presentation_revision()
        && rejected && events.waits == 0, "resume or tick reentry guard failed");
}

} // namespace

int main() {
    try {
        test_requests_coalesce_and_idle_does_not_submit();
        test_event_wakes_idle_and_deferred_frame_does_not_spin();
        test_deadline_wait_rounding_and_event_coalescing();
        test_animation_pipeline_deferred_retry_and_idle_recovery();
        test_dirty_update_requests_a_frame();
        test_nonblocking_callback_requests_and_lifetime();
        test_callback_animation_schedule_changes();
        test_pump_destroyed_during_submission();
        test_deferred_revision_and_reentrant_tick();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
