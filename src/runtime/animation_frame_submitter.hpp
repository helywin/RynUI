#pragma once

#include "animation/runtime.hpp"
#include "runtime/frame_scheduler.hpp"

#include <cstddef>
#include <cstdint>

namespace ryn::runtime {

struct AnimationFrameSubmitterCounters final {
    std::uint64_t animation_ticks{0};
    std::uint64_t animation_updates{0};
    std::uint64_t submitted{0};
    std::uint64_t deferred{0};
    std::uint64_t failed{0};
};

class AnimationFrameSubmitter final : public FrameSubmitter {
public:
    AnimationFrameSubmitter(
        animation::AnimationRuntime& animations,
        FrameSubmitter& downstream) noexcept
        : animations_(&animations), downstream_(&downstream) {}

    FrameSubmissionResult submit_frame(
        animation::AnimationTime frame_time) override {
        ++counters_.animation_ticks;
        counters_.animation_updates += animations_->tick(frame_time);
        const auto result = downstream_->submit_frame(frame_time);
        switch (result) {
        case FrameSubmissionResult::submitted:
            ++counters_.submitted;
            break;
        case FrameSubmissionResult::deferred:
            ++counters_.deferred;
            break;
        case FrameSubmissionResult::failed:
            ++counters_.failed;
            break;
        }
        return result;
    }

    [[nodiscard]] const AnimationFrameSubmitterCounters& counters() const noexcept {
        return counters_;
    }

private:
    animation::AnimationRuntime* animations_;
    FrameSubmitter* downstream_;
    AnimationFrameSubmitterCounters counters_;
};

} // namespace ryn::runtime
