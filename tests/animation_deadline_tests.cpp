#include "animation/runtime.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Operation>
void require_throws(Operation&& operation, const char* message) {
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

struct ScalarSink final : ryn::animation::AnimationTargetSink {
    void apply(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId,
        const ryn::animation::AnimationValue& candidate,
        ryn::animation::AnimationDirtyDomain) override {
        values.push_back(std::get<float>(candidate));
    }

    void completed(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId) override {
        ++completions;
    }

    std::vector<float> values;
    int completions{0};
};

struct CadenceResult final {
    ryn::animation::AnimationTime final_time;
    std::size_t samples{0};
    float value{0.0F};
    int completions{0};
};

CadenceResult run_cadence(std::int64_t period_microseconds) {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(2, 1, 1);
    runtime.set_nominal_frame_period(
        AnimationDuration::microseconds(period_microseconds));
    ScalarSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material | AnimationDirtyDomain::animation);
    static_cast<void>(runtime.play(
        target,
        0.0F,
        1.0F,
        {{}, AnimationDuration::microseconds(1'000'000), Easing::linear()},
        {}));

    AnimationTime final_time;
    std::size_t samples = 0;
    while (const auto deadline = runtime.next_deadline()) {
        final_time = *deadline;
        static_cast<void>(runtime.tick(*deadline));
        ++samples;
    }
    return {final_time, samples, sink.values.back(), sink.completions};
}

void test_60_120_144_hz_reach_the_same_endpoint() {
    const std::array periods{
        std::int64_t{16'667},
        std::int64_t{8'333},
        std::int64_t{6'944},
    };
    std::array<CadenceResult, 3> results;
    for (std::size_t index = 0; index < periods.size(); ++index) {
        results[index] = run_cadence(periods[index]);
        require(results[index].final_time
                    == ryn::animation::AnimationTime::microseconds(1'000'000)
                    && results[index].value == 1.0F
                    && results[index].completions == 1,
                "display cadence changed animation endpoint or completion time");
    }
    require(results[0].samples < results[1].samples
                && results[1].samples < results[2].samples,
            "higher nominal refresh rate did not produce more samples");
}

void test_absolute_deadline_skips_missed_cadence() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(2, 1, 1);
    runtime.set_nominal_frame_period(AnimationDuration::microseconds(1000));
    ScalarSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material);
    static_cast<void>(runtime.play(
        target,
        0.0F,
        10.0F,
        {{}, AnimationDuration::microseconds(10'000), Easing::linear()},
        {}));

    static_cast<void>(runtime.tick(AnimationTime::microseconds(1000)));
    static_cast<void>(runtime.tick(AnimationTime::microseconds(5500)));
    require(runtime.diagnostics().missed_cadences == 3,
            "long stall did not count skipped cadence intervals");
    require(runtime.next_deadline() == AnimationTime::microseconds(6000),
            "long stall queued a historical catch-up frame");
    require(runtime.tick(AnimationTime::microseconds(5000)) == 0
                && runtime.diagnostics().non_monotonic_timestamps == 1,
            "backward deadline timestamp advanced animation state");
}

void test_delay_and_multiple_animation_deadlines() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(4, 1, 2);
    runtime.set_nominal_frame_period(AnimationDuration::microseconds(1000));
    ScalarSink sink;
    const auto scope = runtime.create_scope();
    const auto first = runtime.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material);
    const auto second = runtime.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::geometry);
    static_cast<void>(runtime.play(
        first,
        0.0F,
        1.0F,
        {AnimationDuration::microseconds(5000),
         AnimationDuration::microseconds(1000), Easing::linear()},
        {}));
    static_cast<void>(runtime.play(
        second,
        0.0F,
        1.0F,
        {{}, AnimationDuration::microseconds(3000), Easing::linear()},
        {}));
    require(runtime.next_deadline() == AnimationTime::microseconds(1000),
            "multiple animations did not select the earliest deadline");

    require_throws<std::invalid_argument>(
        [&] { runtime.set_nominal_frame_period({}); },
        "zero nominal frame period was accepted");
}

} // namespace

int main() {
    try {
        test_60_120_144_hz_reach_the_same_endpoint();
        test_absolute_deadline_skips_missed_cadence();
        test_delay_and_multiple_animation_deadlines();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
