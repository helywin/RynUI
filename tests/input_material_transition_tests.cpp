#include "component/input_material_transition.hpp"
#include "animation/motion_policy.hpp"
#include "support/allocation_probe.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::animation;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void transitions() {
    AnimationRuntime runtime; runtime.reserve(64, 4, 64);
    detail::InputMaterialValues initial, goal;
    initial.colors.fill(Color(0, 0, 0)); goal.colors.fill(Color(1, 1, 1)); goal.shadow_opacity = 1;
    int updates{};
    const auto spec = resolve_motion_policy(resolve_theme()).transition(MotionDurationToken::mid, MotionEasingToken::ease_in_out);
    {
        detail::InputMaterialTransition transition{runtime, initial, [&] { ++updates; }};
        transition.retarget(goal, spec, AnimationTime{});
        require(transition.active_count() == detail::input_material_color_count + 1, "Input channels were not scheduled");
        runtime.tick(AnimationTime::microseconds(50000));
        const auto quarter = transition.value().colors[0].red();
        require(quarter > 0 && quarter < 1, "Input transition did not interpolate");
        const auto retargets = runtime.diagnostics().retargeted;
        transition.retarget(goal, spec, AnimationTime::microseconds(50000));
        require(runtime.diagnostics().retargeted == retargets, "Same Input target restarted transition");
        transition.retarget(initial, spec, AnimationTime::microseconds(50000));
        require(transition.value().colors[0].red() == quarter, "Rapid Input retarget jumped presentation");
        runtime.tick(AnimationTime::microseconds(250000));
        require(transition.value() == initial && transition.active_count() == 0 && !runtime.next_deadline(),
            "Input transition did not settle/return idle");
        transition.retarget(goal, spec, AnimationTime::microseconds(300000));
        const auto reduced = resolve_motion_policy(resolve_theme(), MotionPreference::reduced)
            .transition(MotionDurationToken::mid, MotionEasingToken::ease_in_out);
        transition.retarget(goal, reduced, AnimationTime::microseconds(310000));
        require(transition.value() == goal && transition.active_count() == 0, "Reduced motion did not snap Input channels");
        transition.retarget(initial, spec, AnimationTime::microseconds(400000));
    }
    require(runtime.size() == 0 && runtime.diagnostics().targets == 0 && runtime.diagnostics().scopes == 0
        && !runtime.next_deadline() && updates > 0, "Input transition teardown retained work");
}
void allocation() {
    AnimationRuntime runtime; runtime.reserve(64, 4, 64);
    detail::InputMaterialValues initial, goal;
    goal.colors.fill(Color(1, 0, 0)); goal.shadow_opacity = 1;
    const AnimationSpec spec{{}, AnimationDuration::microseconds(100), Easing::linear()};
    detail::InputMaterialTransition transition{runtime, initial, [] {}};
    const auto cycle = [&](std::int64_t index) {
        transition.retarget(index % 2 ? initial : goal, spec, AnimationTime::microseconds(index * 1000));
        runtime.tick(AnimationTime::microseconds(index * 1000 + 100));
    };
    for(std::int64_t i = 0; i < 20; ++i) cycle(i);
    ryn_test::allocation::begin();
    for(std::int64_t i = 20; i < 20020; ++i) cycle(i);
    const auto allocations = ryn_test::allocation::end();
    require(allocations == 0 && !runtime.next_deadline(), "Input transition allocated after warmup or failed to idle");
}
}
int main() {
    try { transitions(); allocation(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
