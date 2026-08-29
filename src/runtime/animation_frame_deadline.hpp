#pragma once

#include "animation/runtime.hpp"
#include "runtime/frame_scheduler.hpp"

namespace ryn::runtime {

class AnimationFrameDeadlineSource final : public FrameDeadlineSource {
public:
    explicit AnimationFrameDeadlineSource(
        animation::AnimationRuntime& runtime) noexcept
        : runtime_(&runtime) {}

    [[nodiscard]] std::optional<animation::AnimationTime>
    next_deadline() const override {
        return runtime_->next_deadline();
    }

private:
    animation::AnimationRuntime* runtime_;
};

} // namespace ryn::runtime
