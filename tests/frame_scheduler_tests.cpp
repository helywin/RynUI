#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"

#include <deque>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ControlledEvents final : public ryn::runtime::FrameEventSource {
public:
    std::uint64_t now_milliseconds() const noexcept override {
        return now;
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
            ++now;
            return true;
        }
        now += timeout_milliseconds;
        return false;
    }

    std::uint64_t now{0};
    bool poll_event{false};
    bool wake_on_wait{false};
};

class ControlledSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ryn::runtime::FrameSubmissionResult submit_frame() override {
        ++calls;
        if (results.empty()) {
            return ryn::runtime::FrameSubmissionResult::submitted;
        }
        const auto result = results.front();
        results.pop_front();
        return result;
    }

    std::deque<ryn::runtime::FrameSubmissionResult> results;
    int calls{0};
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
    require(events.now == 120U * 16U, "controlled clock did not advance through idle waits");
    require(loop.counters().idle_waits == 120, "idle wait counter is incorrect");
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
        test_dirty_update_requests_a_frame();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
