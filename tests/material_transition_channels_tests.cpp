#include "animation/material_transition_channels.hpp"
#include "support/allocation_probe.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

using namespace ryn;
using namespace ryn::animation;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Sink final : AnimationTargetSink {
    AnimationTargetId color_target;
    AnimationTargetId scalar_target;
    Color color{0.0F, 0.0F, 0.0F};
    float scalar{};
    std::size_t updates{};
    bool domains_valid{true};

    void apply(AnimationId, AnimationTargetId target, const AnimationValue& value,
               AnimationDirtyDomain dirty) override {
        domains_valid = domains_valid
            && has_any(dirty, AnimationDirtyDomain::material)
            && has_any(dirty, AnimationDirtyDomain::animation)
            && !has_any(dirty, AnimationDirtyDomain::measure_layout);
        if (target == color_target) color = std::get<Color>(value);
        else if (target == scalar_target) scalar = std::get<float>(value);
        else throw std::runtime_error("unrecognized transition target");
        ++updates;
    }
};

void lifetime_and_retarget() {
    AnimationRuntime runtime;
    runtime.reserve(8, 2, 4);
    Sink sink;
    const auto dirty = AnimationDirtyDomain::material | AnimationDirtyDomain::animation;
    const std::array kinds{AnimationValueKind::color, AnimationValueKind::scalar};
    const AnimationSpec spec{{}, AnimationDuration::microseconds(100'000), Easing::linear()};
    AnimationId color_animation, scalar_animation;
    {
        MaterialTransitionTargets<2> targets{runtime, sink, kinds, dirty};
        sink.color_target = targets.target(0);
        sink.scalar_target = targets.target(1);
        require(runtime.diagnostics().scopes == 1 && runtime.diagnostics().targets == 2,
                "material targets were not registered in one scope");
        retarget_material_channel(runtime, color_animation, sink.color_target,
            sink.color, Color{1.0F, 1.0F, 1.0F}, spec, AnimationTime{});
        retarget_material_channel(runtime, scalar_animation, sink.scalar_target,
            sink.scalar, 1.0F, spec, AnimationTime{});
        require(runtime.size() == 2 && runtime.next_deadline().has_value(),
                "typed material channels did not schedule");
        static_cast<void>(runtime.tick(AnimationTime::microseconds(50'000)));
        const auto middle = sink.color.red();
        require(middle > 0.0F && middle < 1.0F && sink.scalar > 0.0F
                    && sink.scalar < 1.0F && sink.domains_valid,
                "material presentation or dirty domain changed");
        retarget_material_channel(runtime, color_animation, sink.color_target,
            sink.color, Color{0.0F, 0.0F, 0.0F}, spec,
            AnimationTime::microseconds(50'000));
        require(sink.color.red() == middle && runtime.size() == 2,
                "reverse retarget jumped presentation");
        retarget_material_channel(runtime, color_animation, sink.color_target,
            sink.color, sink.color, spec, AnimationTime::microseconds(50'000));
        require(!runtime.contains(color_animation),
                "same-value material channel did not settle");
        require(runtime.contains(scalar_animation),
                "settling one channel disturbed its sibling");
    }
    require(runtime.size() == 0 && runtime.diagnostics().scopes == 0
                && runtime.diagnostics().targets == 0 && !runtime.next_deadline(),
            "material scope disposal retained animation work");
}

void failed_registration_rolls_back() {
    AnimationRuntime runtime;
    runtime.reserve(2, 1, 2);
    Sink sink;
    const std::array kinds{
        AnimationValueKind::color, static_cast<AnimationValueKind>(255)};
    bool rejected = false;
    try {
        MaterialTransitionTargets<2> targets{runtime, sink, kinds,
            AnimationDirtyDomain::material | AnimationDirtyDomain::animation};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && runtime.diagnostics().scopes == 0
                && runtime.diagnostics().targets == 0 && !runtime.next_deadline(),
            "partial target registration did not roll back");
}

void steady_state_allocation() {
    AnimationRuntime runtime;
    runtime.reserve(4, 1, 1);
    Sink sink;
    const std::array kinds{AnimationValueKind::scalar};
    MaterialTransitionTargets<1> targets{runtime, sink, kinds,
        AnimationDirtyDomain::material | AnimationDirtyDomain::animation};
    sink.scalar_target = targets.target(0);
    AnimationId active;
    const AnimationSpec spec{{}, AnimationDuration::microseconds(100), Easing::linear()};
    const auto cycle = [&](std::int64_t index) {
        const float target = index % 2 ? 0.0F : 1.0F;
        retarget_material_channel(runtime, active, sink.scalar_target,
            sink.scalar, target, spec, AnimationTime::microseconds(index * 1'000));
        static_cast<void>(runtime.tick(AnimationTime::microseconds(index * 1'000 + 100)));
    };
    for (std::int64_t index = 0; index < 20; ++index) cycle(index);
    ryn_test::allocation::begin();
    for (std::int64_t index = 20; index < 20'020; ++index) cycle(index);
    const auto allocations = ryn_test::allocation::end();
    require(allocations == 0 && !runtime.next_deadline(),
            "material channels allocated after warmup or failed to idle");
}

} // namespace

int main() {
    try {
        lifetime_and_retarget();
        failed_registration_rolls_back();
        steady_state_allocation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
