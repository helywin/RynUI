#pragma once

#include "animation/material_transition_channels.hpp"
#include "animation/motion_policy.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {
struct NavigationBindings final {
    std::unique_ptr<Scope> scope;

    Scope& reset() {
        scope = std::make_unique<Scope>();
        return *scope;
    }

    void dispose() {
        scope.reset();
    }
};

// A finite, component-owned activation envelope. Target generations are retired
// by MaterialTransitionTargets before the component state is released.
class NavigationTransition final : private animation::AnimationTargetSink {
public:
    NavigationTransition(WindowComponentServices& services, runtime::ComponentId owner)
        : services_(&services), owner_(owner),
          targets_(services.animations(), *this, {animation::AnimationValueKind::scalar},
                   animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation) {}

    void restart(bool enabled) {
        settle();
        const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(owner_)->snapshot(),
                                                             services_->motion_preference());
        if (!enabled || !policy.enabled() || !services_->components().branch_active(owner_) ||
            !services_->focus().state().window_active) {
            return;
        }
        value_ = 0;
        animation_ = services_->animations().play(
            targets_.target(0), 0.0F, 1.0F,
            policy.transition(animation::MotionDurationToken::fast, animation::MotionEasingToken::ease_out),
            services_->animation_time());
    }

    void settle() {
        if (services_->animations().contains(animation_)) {
            static_cast<void>(services_->animations().finish(animation_));
        }
        value_ = 1;
    }

    [[nodiscard]] float value() const noexcept {
        return value_;
    }

private:
    void apply(animation::AnimationId, animation::AnimationTargetId, const animation::AnimationValue& value,
               animation::AnimationDirtyDomain) override {
        value_ = std::get<float>(value);
        if (services_->components().contains(owner_)) {
            services_->dirty().invalidate(services_->components().root(owner_), runtime::DirtyFlags::Material);
        }
    }

    WindowComponentServices* services_;
    runtime::ComponentId owner_;
    animation::MaterialTransitionTargets<1> targets_;
    animation::AnimationId animation_;
    float value_{1};
};
} // namespace ryn::detail
