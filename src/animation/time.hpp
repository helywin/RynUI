#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace ryn::animation {

class AnimationDuration final {
public:
    using rep = std::int64_t;

    constexpr AnimationDuration() noexcept = default;

    [[nodiscard]] static constexpr AnimationDuration microseconds(rep value) {
        if (value < 0) {
            throw std::invalid_argument(
                "animation duration must be non-negative");
        }
        return AnimationDuration(value);
    }

    [[nodiscard]] static AnimationDuration milliseconds(double value);

    [[nodiscard]] constexpr rep count_microseconds() const noexcept {
        return microseconds_;
    }

    friend constexpr auto operator<=>(
        AnimationDuration,
        AnimationDuration) = default;

private:
    explicit constexpr AnimationDuration(rep value) noexcept
        : microseconds_(value) {}

    rep microseconds_{0};
};

class AnimationTime final {
public:
    using rep = std::int64_t;

    constexpr AnimationTime() noexcept = default;

    [[nodiscard]] static constexpr AnimationTime microseconds(rep value) {
        if (value < 0) {
            throw std::invalid_argument("animation time must be non-negative");
        }
        return AnimationTime(value);
    }

    [[nodiscard]] constexpr rep count_microseconds() const noexcept {
        return microseconds_;
    }

    friend constexpr auto operator<=>(AnimationTime, AnimationTime) = default;

private:
    explicit constexpr AnimationTime(rep value) noexcept : microseconds_(value) {}

    rep microseconds_{0};
};

[[nodiscard]] AnimationTime operator+(
    AnimationTime time,
    AnimationDuration duration);
[[nodiscard]] AnimationDuration operator-(
    AnimationTime later,
    AnimationTime earlier);

class AnimationClock {
public:
    virtual ~AnimationClock() = default;
    [[nodiscard]] virtual AnimationTime now() const noexcept = 0;
};

class SteadyAnimationClock final : public AnimationClock {
public:
    SteadyAnimationClock() noexcept;
    [[nodiscard]] AnimationTime now() const noexcept override;

private:
    std::chrono::steady_clock::time_point origin_;
};

class ControlledAnimationClock final : public AnimationClock {
public:
    explicit constexpr ControlledAnimationClock(
        AnimationTime initial = {}) noexcept
        : current_(initial) {}

    [[nodiscard]] AnimationTime now() const noexcept override;
    void set(AnimationTime value) noexcept;
    void advance(AnimationDuration duration);

private:
    AnimationTime current_;
};

struct AnimationTimeObservation final {
    AnimationTime effective;
    bool advanced{false};
    bool clamped{false};
};

class MonotonicTimeCursor final {
public:
    [[nodiscard]] AnimationTimeObservation observe(AnimationTime candidate) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::optional<AnimationTime> last() const noexcept;

private:
    std::optional<AnimationTime> last_;
};

enum class AnimationIntervalPhase : std::uint8_t {
    delayed,
    active,
    completed,
};

struct AnimationIntervalSample final {
    AnimationIntervalPhase phase{AnimationIntervalPhase::delayed};
    float progress{0.0F};

    friend constexpr bool operator==(
        AnimationIntervalSample,
        AnimationIntervalSample) = default;
};

[[nodiscard]] AnimationIntervalSample sample_animation_interval(
    AnimationTime sample_time,
    AnimationTime start_time,
    AnimationDuration delay,
    AnimationDuration duration);

} // namespace ryn::animation
