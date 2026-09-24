#pragma once

#include "animation/runtime.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace ryn::animation {

// Retained targets only. The consumer still owns its presentation values,
// animation IDs, dirty-domain declaration, and any repeating animation policy.
template<std::size_t ChannelCount>
class MaterialTransitionTargets final {
    static_assert(ChannelCount > 0);
public:
    MaterialTransitionTargets(
        AnimationRuntime& runtime,
        AnimationTargetSink& sink,
        const std::array<AnimationValueKind, ChannelCount>& kinds,
        AnimationDirtyDomain dirty_domain)
        : runtime_(&runtime), scope_(runtime.create_scope()) {
        try {
            for (std::size_t index = 0; index < ChannelCount; ++index) {
                targets_[index] = runtime.register_target(
                    scope_, sink, kinds[index], dirty_domain);
            }
        } catch (...) {
            static_cast<void>(runtime.dispose_scope(scope_));
            scope_ = {};
            throw;
        }
    }

    ~MaterialTransitionTargets() noexcept {
        if (scope_.valid()) {
            try {
                static_cast<void>(runtime_->dispose_scope(scope_));
            } catch (...) {
            }
        }
    }

    MaterialTransitionTargets(const MaterialTransitionTargets&) = delete;
    MaterialTransitionTargets& operator=(const MaterialTransitionTargets&) = delete;
    MaterialTransitionTargets(MaterialTransitionTargets&&) = delete;
    MaterialTransitionTargets& operator=(MaterialTransitionTargets&&) = delete;

    [[nodiscard]] AnimationTargetId target(std::size_t index) const {
        return targets_.at(index);
    }
    [[nodiscard]] const std::array<AnimationTargetId, ChannelCount>& targets()
        const noexcept { return targets_; }

private:
    AnimationRuntime* runtime_;
    AnimationScopeId scope_;
    std::array<AnimationTargetId, ChannelCount> targets_{};
};

inline void retarget_material_channel(
    AnimationRuntime& runtime,
    AnimationId& active,
    AnimationTargetId target,
    AnimationValue current,
    AnimationValue destination,
    const AnimationSpec& spec,
    AnimationTime time) {
    if (current == destination) {
        if (runtime.contains(active)) {
            static_cast<void>(runtime.retarget(
                active, std::move(destination), {{}, {}, spec.easing}, time));
        }
        return;
    }
    if (runtime.contains(active)) {
        static_cast<void>(runtime.retarget(
            active, std::move(destination), spec, time));
    } else {
        active = runtime.play(
            target, std::move(current), std::move(destination), spec, time);
    }
}

} // namespace ryn::animation
