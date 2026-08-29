#include "animation/time.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
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

void test_duration_and_large_time_boundaries() {
    using ryn::animation::AnimationDuration;
    using ryn::animation::AnimationTime;

    require(AnimationDuration::milliseconds(1.25).count_microseconds() == 1250,
            "millisecond conversion lost microsecond precision");
    require_throws<std::invalid_argument>(
        [] { static_cast<void>(AnimationDuration::milliseconds(-1.0)); },
        "negative duration was accepted");
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(AnimationDuration::milliseconds(
                std::numeric_limits<double>::quiet_NaN()));
        },
        "NaN duration was accepted");
    require_throws<std::overflow_error>(
        [] {
            static_cast<void>(AnimationDuration::milliseconds(
                std::numeric_limits<double>::max()));
        },
        "overflowing duration was accepted");

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto near_maximum = AnimationTime::microseconds(maximum - 100);
    const auto advanced = near_maximum + AnimationDuration::microseconds(100);
    require(advanced.count_microseconds() == maximum,
            "large animation time did not reach the exact boundary");
    require_throws<std::overflow_error>(
        [&] {
            static_cast<void>(
                near_maximum + AnimationDuration::microseconds(101));
        },
        "animation time addition overflow was not rejected");
    require_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                AnimationTime::microseconds(1) - AnimationTime::microseconds(2));
        },
        "negative animation time difference was accepted");
}

void test_controlled_clock_and_monotonic_cursor() {
    using namespace ryn::animation;

    ControlledAnimationClock clock(AnimationTime::microseconds(10));
    MonotonicTimeCursor cursor;
    const auto first = cursor.observe(clock.now());
    require(first.effective == AnimationTime::microseconds(10)
                && first.advanced && !first.clamped,
            "first controlled timestamp was not accepted");

    const auto repeated = cursor.observe(clock.now());
    require(repeated.effective == first.effective
                && !repeated.advanced && !repeated.clamped,
            "same timestamp was not idempotent");

    clock.set(AnimationTime::microseconds(5));
    const auto backwards = cursor.observe(clock.now());
    require(backwards.effective == AnimationTime::microseconds(10)
                && !backwards.advanced && backwards.clamped,
            "backward timestamp was not clamped");

    clock.set(AnimationTime::microseconds(25));
    const auto forward = cursor.observe(clock.now());
    require(forward.effective == AnimationTime::microseconds(25)
                && forward.advanced && !forward.clamped,
            "forward timestamp was not observed");
    cursor.reset();
    require(!cursor.last().has_value(), "monotonic cursor did not reset");
}

std::vector<ryn::animation::AnimationIntervalSample> replay_interval() {
    using namespace ryn::animation;
    ControlledAnimationClock clock(AnimationTime::microseconds(1000));
    std::vector<AnimationIntervalSample> result;
    const auto start = clock.now();
    for (const auto offset : {0, 500, 1000, 1500, 3000}) {
        clock.set(start + AnimationDuration::microseconds(offset));
        result.push_back(sample_animation_interval(
            clock.now(),
            start,
            AnimationDuration::microseconds(1000),
            AnimationDuration::microseconds(2000)));
    }
    return result;
}

void test_interval_endpoints_and_deterministic_replay() {
    using namespace ryn::animation;
    const auto start = AnimationTime::microseconds(1000);
    const auto delay = AnimationDuration::microseconds(1000);
    const auto duration = AnimationDuration::microseconds(2000);

    require(sample_animation_interval(start, start, delay, duration)
                == AnimationIntervalSample{AnimationIntervalPhase::delayed, 0.0F},
            "interval did not preserve its start value during delay");
    require(sample_animation_interval(
                AnimationTime::microseconds(2000), start, delay, duration)
                == AnimationIntervalSample{AnimationIntervalPhase::active, 0.0F},
            "interval active start was not exact");
    require(sample_animation_interval(
                AnimationTime::microseconds(3000), start, delay, duration)
                == AnimationIntervalSample{AnimationIntervalPhase::active, 0.5F},
            "interval midpoint was not exact");
    require(sample_animation_interval(
                AnimationTime::microseconds(4000), start, delay, duration)
                == AnimationIntervalSample{AnimationIntervalPhase::completed, 1.0F},
            "interval end was not exact");
    require(sample_animation_interval(start, start, {}, {})
                == AnimationIntervalSample{AnimationIntervalPhase::completed, 1.0F},
            "zero-duration interval did not complete immediately");
    require(replay_interval() == replay_interval(),
            "controlled interval replay was not deterministic");

    SteadyAnimationClock production;
    const auto first = production.now();
    const auto second = production.now();
    require(second >= first, "production steady clock moved backwards");
}

} // namespace

int main() {
    try {
        test_duration_and_large_time_boundaries();
        test_controlled_clock_and_monotonic_cursor();
        test_interval_endpoints_and_deterministic_replay();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
