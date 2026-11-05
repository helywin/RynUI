#pragma once

#include "animation/motion_policy.hpp"
#include <ryn/async_options.hpp>
#include <algorithm>
#include <cmath>
#include <thread>

namespace ryn::detail {

struct WindowedRange final {
    std::size_t first{};
    std::size_t end{};
};

inline WindowedRange windowed_range(std::size_t count, float scroll, float height, float row_height, bool enabled) {
    if (!enabled || count <= 32) {
        return {0, count};
    }
    const auto first = std::min(count, static_cast<std::size_t>(std::max(0.0F, scroll) / row_height));
    const auto visible = static_cast<std::size_t>(std::ceil(std::max(0.0F, height) / row_height));
    return {first > 1 ? first - 1 : 0, std::min(count, first + visible + 2)};
}

// Only immutable tickets cross threads. Host captures are invoked exclusively by the
// application dispatcher on the creating thread, after cancellation/lifetime checks.
template <class Option> class AsyncOptionsSession final {
    struct Ticket final {
        std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
        std::atomic_bool completed{};
        std::thread::id owner{std::this_thread::get_id()};
    };

    struct Control final {
        std::shared_ptr<Ticket> ticket;
        AsyncOptionsCancel cancel;
        std::uint64_t generation{};
    };

public:
    ~AsyncOptionsSession() {
        cancel();
    }

    // Invalidate all members of a compound operation before invoking application cancellation code.
    [[nodiscard]] AsyncOptionsCancel detach_cancel() noexcept {
        const auto control = control_;
        ++control->generation;
        if (control->ticket) {
            control->ticket->cancelled->store(true);
            control->ticket.reset();
        }
        return std::move(control->cancel);
    }

    void cancel() noexcept {
        auto cancel = detach_cancel();
        if (cancel) {
            try {
                cancel();
            } catch (...) {
            }
        }
    }

    void start(const AsyncOptionsSource<Option>& source, String query,
               std::function<void(AsyncOptionsResult<Option>)> apply) {
        const auto control = control_;
        const auto generation = control->generation + 1;
        cancel();
        // Cancellation is application code: it can start a newer request or
        // destroy this session. Do not access this again or replace that request.
        if (control->generation != generation || !source) {
            return;
        }
        const auto ticket = std::make_shared<Ticket>();
        control->ticket = ticket;
        const auto weak = std::weak_ptr<Ticket>{ticket};
        const auto provider = source.provider();
        const auto post = source.post();
        auto cancel =
            provider({std::move(query), generation, ticket->cancelled},
                     [weak, post, apply = std::move(apply)](AsyncOptionsResult<Option> result) {
                         const auto ticket = weak.lock();
                         if (!ticket || ticket->cancelled->load()) {
                             return;
                         }
                         post([weak, result = std::move(result), apply]() mutable {
                             const auto ticket = weak.lock();
                             if (!ticket || ticket->cancelled->load() || std::this_thread::get_id() != ticket->owner ||
                                 ticket->completed.exchange(true)) {
                                 return;
                             }
                             apply(std::move(result));
                         });
                     });
        // Synchronous providers may re-enter or destroy the owner before returning.
        if (ticket->cancelled->load()) {
            if (cancel) {
                try {
                    cancel();
                } catch (...) {
                }
            }
        } else {
            control->cancel = std::move(cancel);
        }
    }

private:
    std::shared_ptr<Control> control_{std::make_shared<Control>()};
};

struct OptionPopupMotion final {
    animation::MonotonicTimeCursor clock;
    std::optional<animation::AnimationTime> start;
    animation::AnimationDuration duration;
    float opacity{1};

    void stop() {
        start.reset();
        opacity = 1;
    }

    void begin(const ThemeSnapshot& theme, animation::MotionPreference preference, animation::AnimationTime now,
               bool enabled) {
        stop();
        const auto policy = animation::resolve_motion_policy(theme, preference);
        duration = policy.tokens().fast;
        if (enabled && policy.enabled() && duration.count_microseconds() > 0) {
            start = clock.observe(now).effective;
            opacity = 0;
        }
    }

    bool tick(animation::AnimationTime now) {
        if (!start) {
            return false;
        }
        const auto sample = animation::sample_animation_interval(clock.observe(now).effective, *start, {}, duration);
        opacity = sample.progress;
        if (sample.phase == animation::AnimationIntervalPhase::completed) {
            stop();
        }
        return true;
    }

    [[nodiscard]] std::optional<animation::AnimationTime> deadline() const {
        return start ? std::optional{clock.last().value_or(*start) + animation::AnimationDuration::milliseconds(16)}
                     : std::nullopt;
    }
};

} // namespace ryn::detail
