#include "animation/motion_policy.hpp"

#include <iostream>
#include <stdexcept>

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

struct ScalarSink final : ryn::animation::AnimationTargetSink {
    void apply(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId,
        const ryn::animation::AnimationValue& candidate,
        ryn::animation::AnimationDirtyDomain) override {
        value = std::get<float>(candidate);
        ++applies;
    }

    void completed(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId) override {
        ++completions;
    }

    float value{0.0F};
    int applies{0};
    int completions{0};
};

void test_default_tokens_and_ant_easing_identity() {
    using namespace ryn::animation;
    const auto tokens = resolve_motion_tokens(ryn::resolve_theme());
    require(tokens.unit == AnimationDuration::microseconds(100'000)
                && tokens.base == AnimationDuration{}
                && tokens.fast == AnimationDuration::microseconds(100'000)
                && tokens.mid == AnimationDuration::microseconds(200'000)
                && tokens.slow == AnimationDuration::microseconds(300'000)
                && tokens.theme_motion_enabled,
            "default Ant Design motion durations drifted");
    require(tokens.easing(MotionEasingToken::ease_out_circ)
                    == ant_easing(AntEasingPreset::ease_out_circ)
                && tokens.easing(MotionEasingToken::ease_in_out_circ)
                    == ant_easing(AntEasingPreset::ease_in_out_circ)
                && tokens.easing(MotionEasingToken::ease_out)
                    == ant_easing(AntEasingPreset::ease_out)
                && tokens.easing(MotionEasingToken::ease_in_out)
                    == ant_easing(AntEasingPreset::ease_in_out)
                && tokens.easing(MotionEasingToken::ease_out_back)
                    == ant_easing(AntEasingPreset::ease_out_back)
                && tokens.easing(MotionEasingToken::ease_in_back)
                    == ant_easing(AntEasingPreset::ease_in_back)
                && tokens.easing(MotionEasingToken::ease_in_quint)
                    == ant_easing(AntEasingPreset::ease_in_quint)
                && tokens.easing(MotionEasingToken::ease_out_quint)
                    == ant_easing(AntEasingPreset::ease_out_quint),
            "typed MotionTokenSet easing identity drifted");
}

void test_override_algorithm_and_nested_theme_mapping() {
    using namespace ryn::animation;
    ryn::ThemeConfig override;
    override.seed.motion_base = ryn::Duration::milliseconds(20.0F);
    override.seed.motion_unit = ryn::Duration::milliseconds(50.0F);
    const auto parent = ryn::resolve_theme(override);
    const auto overridden = resolve_motion_tokens(parent);
    require(overridden.fast == AnimationDuration::microseconds(70'000)
                && overridden.mid == AnimationDuration::microseconds(120'000)
                && overridden.slow == AnimationDuration::microseconds(170'000),
            "motionBase + motionUnit duration derivation is incorrect");

    const auto inherited = resolve_motion_tokens(ryn::resolve_theme({}, &parent));
    require(inherited == overridden,
            "nested Theme lost inherited motion Token values");

    override.algorithms = {
        ryn::ThemeAlgorithm::Dark,
        ryn::ThemeAlgorithm::Compact,
    };
    require(resolve_motion_tokens(ryn::resolve_theme(override)) == overridden,
            "visual Theme algorithms changed motion Token derivation");
}

void test_disabled_and_reduced_policy_specs() {
    using namespace ryn::animation;
    const auto theme = ryn::resolve_theme();
    const auto normal = resolve_motion_policy(theme);
    const auto normal_spec = normal.transition(
        MotionDurationToken::mid,
        MotionEasingToken::ease_in_out,
        AnimationDuration::microseconds(25'000));
    require(normal.enabled() && !normal.reduced()
                && normal_spec.delay == AnimationDuration::microseconds(25'000)
                && normal_spec.duration == AnimationDuration::microseconds(200'000)
                && normal_spec.easing == ant_easing(AntEasingPreset::ease_in_out),
            "normal MotionPolicy did not retain typed duration/easing/delay");

    const auto reduced = resolve_motion_policy(theme, MotionPreference::reduced);
    const auto reduced_spec = reduced.transition(
        MotionDurationToken::slow,
        MotionEasingToken::ease_out_back,
        AnimationDuration::microseconds(50'000));
    require(!reduced.enabled() && reduced.reduced()
                && reduced_spec.delay == AnimationDuration{}
                && reduced_spec.duration == AnimationDuration{}
                && reduced_spec.easing == ant_easing(AntEasingPreset::ease_out_back),
            "reduced MotionPolicy did not remove continuous timing");

    ryn::ThemeConfig disabled_config;
    disabled_config.seed.motion = false;
    const auto disabled = resolve_motion_policy(ryn::resolve_theme(disabled_config));
    require(!disabled.enabled() && !disabled.reduced()
                && disabled.transition(
                       MotionDurationToken::fast,
                       MotionEasingToken::ease_out)
                       .duration == AnimationDuration{},
            "Theme motion=false did not resolve to zero duration");

    require_throws<std::invalid_argument>(
        [&] {
            static_cast<void>(normal.tokens().duration(
                static_cast<MotionDurationToken>(255)));
        },
        "invalid motion duration token was accepted");
    require_throws<std::invalid_argument>(
        [&] {
            static_cast<void>(normal.tokens().easing(
                static_cast<MotionEasingToken>(255)));
        },
        "invalid motion easing token was accepted");
}

void test_runtime_policy_toggle_finishes_active_animation() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(4, 1, 1);
    ScalarSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material | AnimationDirtyDomain::animation);
    const auto theme = ryn::resolve_theme();
    MotionPolicyController controller(runtime, resolve_motion_policy(theme));
    const auto id = runtime.play(
        target,
        0.0F,
        1.0F,
        controller.policy().transition(
            MotionDurationToken::mid,
            MotionEasingToken::ease_in_out),
        {});
    require(runtime.contains(id) && runtime.next_deadline().has_value(),
            "normal policy did not create active animation work");

    require(controller.update(resolve_motion_policy(
                theme, MotionPreference::reduced)),
            "runtime reduced policy update was suppressed");
    require(!runtime.contains(id) && runtime.size() == 0
                && !runtime.next_deadline().has_value()
                && sink.value == 1.0F && sink.completions == 1,
            "reduced policy did not snap active animation to its final state");
    require(!controller.update(resolve_motion_policy(
                theme, MotionPreference::reduced)),
            "equal effective policy update was not suppressed");

    const auto zero = runtime.play(
        target,
        1.0F,
        2.0F,
        controller.policy().transition(
            MotionDurationToken::slow,
            MotionEasingToken::ease_out_back),
        {});
    require(!runtime.contains(zero) && sink.value == 2.0F
                && !runtime.next_deadline().has_value(),
            "reduced policy specification created continuous animation work");
}

void test_generated_motion_metadata_is_runtime_typed() {
    for (const auto identity : {
            "ant.seed.motionEaseOutCirc",
            "ant.seed.motionEaseInOutCirc",
            "ant.seed.motionEaseOut",
            "ant.seed.motionEaseInOut",
            "ant.seed.motionEaseOutBack",
            "ant.seed.motionEaseInBack",
            "ant.seed.motionEaseInQuint",
            "ant.seed.motionEaseOutQuint"}) {
        const auto* token = ryn::find_ant_design_token(identity);
        require(token != nullptr
                    && token->support == ryn::TokenSupportStatus::runtime
                    && token->value_kind == ryn::TokenValueKind::cubic_bezier
                    && token->invalidation == ryn::TokenInvalidationDomain::animation,
                "generated motion easing metadata is not runtime typed");
    }
}

} // namespace

int main() {
    try {
        test_default_tokens_and_ant_easing_identity();
        test_override_algorithm_and_nested_theme_mapping();
        test_disabled_and_reduced_policy_specs();
        test_runtime_policy_toggle_finishes_active_animation();
        test_generated_motion_metadata_is_runtime_typed();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
