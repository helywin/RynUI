#pragma once
#include "renderer/common/scene_resources.hpp"

namespace ryn::detail {
// A finite resource ticket shares the existing host deadline mechanism. It is
// never a second input stack or an unconditional animation/frame request.
class SceneResourceDeadlines final : public runtime::FrameDeadlineSource {
public:
    SceneResourceDeadlines(const runtime::FrameDeadlineSource& animation, const SceneResources& resources)
        : animation_(&animation), resources_(&resources) {}

    [[nodiscard]] std::optional<animation::AnimationTime> next_deadline() const override {
        const auto a = animation_->next_deadline();
        const auto b = resources_->next_deadline();
        if (!a) {
            return b;
        }
        if (!b) {
            return a;
        }
        return *a < *b ? a : b;
    }

private:
    const runtime::FrameDeadlineSource* animation_;
    const SceneResources* resources_;
};
} // namespace ryn::detail
