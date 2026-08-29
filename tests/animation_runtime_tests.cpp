#include "animation/runtime.hpp"

#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

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

struct RecordingSink final : ryn::animation::AnimationTargetSink {
    void apply(
        ryn::animation::AnimationId animation,
        ryn::animation::AnimationTargetId target,
        const ryn::animation::AnimationValue& value,
        ryn::animation::AnimationDirtyDomain dirty_domain) override {
        animations.push_back(animation);
        targets.push_back(target);
        values.push_back(value);
        domains.push_back(dirty_domain);
        if (cancel_self && runtime != nullptr) {
            cancel_self = false;
            static_cast<void>(runtime->cancel(animation, callback_time));
        }
        if (cancel_sibling && runtime != nullptr && sibling.valid()) {
            cancel_sibling = false;
            static_cast<void>(runtime->cancel(sibling, callback_time));
        }
        if (dispose_target && runtime != nullptr) {
            dispose_target = false;
            static_cast<void>(runtime->unregister_target(target));
        }
        if (create_during_apply && runtime != nullptr) {
            create_during_apply = false;
            created = runtime->play(
                create_target,
                100.0F,
                200.0F,
                {{}, ryn::animation::AnimationDuration::microseconds(1000),
                 ryn::animation::Easing::linear()},
                callback_time);
        }
    }

    void completed(
        ryn::animation::AnimationId animation,
        ryn::animation::AnimationTargetId) override {
        completions.push_back(animation);
    }

    ryn::animation::AnimationRuntime* runtime{nullptr};
    ryn::animation::AnimationTime callback_time;
    ryn::animation::AnimationId sibling;
    ryn::animation::AnimationTargetId create_target;
    ryn::animation::AnimationId created;
    bool cancel_self{false};
    bool cancel_sibling{false};
    bool dispose_target{false};
    bool create_during_apply{false};
    std::vector<ryn::animation::AnimationId> animations;
    std::vector<ryn::animation::AnimationTargetId> targets;
    std::vector<ryn::animation::AnimationValue> values;
    std::vector<ryn::animation::AnimationDirtyDomain> domains;
    std::vector<ryn::animation::AnimationId> completions;
};

ryn::animation::AnimationSpec linear_spec(
    std::int64_t duration,
    std::int64_t delay = 0) {
    return {
        ryn::animation::AnimationDuration::microseconds(delay),
        ryn::animation::AnimationDuration::microseconds(duration),
        ryn::animation::Easing::linear(),
    };
}

void test_scope_target_generation_and_dirty_validation() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(8, 2, 4);
    RecordingSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope,
        sink,
        AnimationValueKind::scalar,
        AnimationDirtyDomain::material | AnimationDirtyDomain::animation);
    require(runtime.contains(scope) && runtime.contains(target),
            "live animation scope or target was not registered");

    require_throws<std::invalid_argument>(
        [&] {
            static_cast<void>(runtime.register_target(
                scope,
                sink,
                AnimationValueKind::scalar,
                AnimationDirtyDomain::structure));
        },
        "Structure dirty target was accepted");
    require_throws<std::invalid_argument>(
        [&] {
            static_cast<void>(runtime.register_target(
                scope,
                sink,
                AnimationValueKind::scalar,
                AnimationDirtyDomain::measure_layout));
        },
        "Measure/Layout dirty target was accepted");

    require(runtime.unregister_target(target), "live target did not unregister");
    const auto reused = runtime.register_target(
        scope,
        sink,
        AnimationValueKind::scalar,
        AnimationDirtyDomain::material);
    require(reused.index == target.index && reused.generation != target.generation,
            "target slot reuse did not advance generation");
    require(!runtime.unregister_target(target),
            "stale target operation reached the reused slot");
    require(runtime.contains(reused), "stale target operation removed live target");

    const auto scoped_animation = runtime.play(
        reused, 0.0F, 1.0F, linear_spec(1000), {});

    require(runtime.dispose_scope(scope), "live scope did not dispose");
    require(!runtime.contains(scope) && !runtime.contains(reused)
                && !runtime.contains(scoped_animation),
            "scope disposal left target or animation identity live");
    const auto next_scope = runtime.create_scope();
    require(next_scope.index == scope.index
                && next_scope.generation != scope.generation,
            "scope slot reuse did not advance generation");
}

void test_owner_thread_guard() {
    ryn::animation::AnimationRuntime runtime;
    std::atomic<bool> rejected{false};
    std::thread worker([&] {
        try {
            runtime.reserve(1, 1, 1);
        } catch (const std::logic_error&) {
            rejected.store(true, std::memory_order_relaxed);
        }
    });
    worker.join();
    require(rejected.load(std::memory_order_relaxed),
            "wrong-thread AnimationRuntime access was not rejected");
}

void test_delay_cancel_finish_and_retarget() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(8, 1, 1);
    RecordingSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar, AnimationDirtyDomain::material);
    const auto start = AnimationTime::microseconds(1000);
    const auto delayed = runtime.play(target, 0.0F, 10.0F, linear_spec(1000, 500), start);
    require(sink.values.size() == 1 && std::get<float>(sink.values.back()) == 0.0F,
            "play did not apply the typed start value");
    require(runtime.tick(AnimationTime::microseconds(1250)) == 0,
            "delay reapplied an unchanged start value");
    require(runtime.tick(AnimationTime::microseconds(2000)) == 1
                && std::get<float>(sink.values.back()) == 5.0F,
            "delayed animation midpoint is incorrect");
    require(runtime.cancel(delayed, AnimationTime::microseconds(2250))
                && std::get<float>(sink.values.back()) == 7.5F,
            "cancel did not preserve the current sampled value");
    require(!runtime.contains(delayed), "canceled animation remained active");

    const auto finish_id = runtime.play(
        target, 10.0F, 30.0F, linear_spec(1000),
        AnimationTime::microseconds(3000));
    require(finish_id.index == delayed.index
                && finish_id.generation != delayed.generation,
            "animation slot reuse did not advance generation");
    require(!runtime.retarget(
                delayed, 99.0F, linear_spec(1000),
                AnimationTime::microseconds(3000))
                && runtime.contains(finish_id),
            "stale retarget reached a reused animation slot");
    require(runtime.finish(finish_id)
                && std::get<float>(sink.values.back()) == 30.0F
                && sink.completions.size() == 1,
            "finish did not submit the exact endpoint once");
    require(!runtime.contains(finish_id), "finished animation remained active");

    const auto retarget_id = runtime.play(
        target, 0.0F, 10.0F, linear_spec(1000),
        AnimationTime::microseconds(4000));
    static_cast<void>(runtime.tick(AnimationTime::microseconds(4500)));
    require(std::get<float>(sink.values.back()) == 5.0F,
            "pre-retarget current value is incorrect");
    const auto apply_count = sink.values.size();
    require(runtime.retarget(
                retarget_id, 20.0F, linear_spec(1000),
                AnimationTime::microseconds(4500)),
            "live animation did not retarget");
    require(sink.values.size() == apply_count,
            "retarget introduced a discontinuous duplicate apply");
    static_cast<void>(runtime.tick(AnimationTime::microseconds(5000)));
    require(std::get<float>(sink.values.back()) == 12.5F,
            "retarget did not interpolate from the current sample");

    const auto zero = runtime.play(
        target, 1.0F, 9.0F, linear_spec(0),
        AnimationTime::microseconds(6000));
    require(!runtime.contains(zero) && std::get<float>(sink.values.back()) == 9.0F,
            "zero-duration animation created persistent work");
}

void test_callback_reentrancy_and_tick_snapshot() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(12, 1, 2);
    RecordingSink first_sink;
    RecordingSink second_sink;
    first_sink.runtime = &runtime;
    second_sink.runtime = &runtime;
    const auto scope = runtime.create_scope();
    const auto first_target = runtime.register_target(
        scope, first_sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material);
    const auto second_target = runtime.register_target(
        scope, second_sink, AnimationValueKind::scalar,
        AnimationDirtyDomain::material);

    first_sink.cancel_self = true;
    first_sink.callback_time = AnimationTime::microseconds(0);
    const auto self = runtime.play(
        first_target, 0.0F, 1.0F, linear_spec(1000), {});
    require(!runtime.contains(self),
            "apply callback could not safely cancel its own animation");

    const auto sibling = runtime.play(
        second_target, 0.0F, 1.0F, linear_spec(1000),
        AnimationTime::microseconds(1000));
    first_sink.sibling = sibling;
    first_sink.cancel_sibling = true;
    first_sink.callback_time = AnimationTime::microseconds(1000);
    const auto owner = runtime.play(
        first_target, 0.0F, 1.0F, linear_spec(1000),
        AnimationTime::microseconds(1000));
    require(!runtime.contains(sibling) && runtime.contains(owner),
            "callback sibling cancellation corrupted the active owner");

    first_sink.create_target = second_target;
    first_sink.create_during_apply = true;
    first_sink.callback_time = AnimationTime::microseconds(1500);
    const auto before = second_sink.values.size();
    static_cast<void>(runtime.tick(AnimationTime::microseconds(1500)));
    require(first_sink.created.valid() && runtime.contains(first_sink.created)
                && second_sink.values.size() == before + 1
                && std::get<float>(second_sink.values.back()) == 100.0F,
            "callback-created animation did not remain at its initial value");
    static_cast<void>(runtime.tick(AnimationTime::microseconds(2000)));
    require(std::get<float>(second_sink.values.back()) == 150.0F,
            "callback-created animation was not deferred to the next tick snapshot");

    first_sink.dispose_target = true;
    first_sink.callback_time = AnimationTime::microseconds(2100);
    const auto disposable = runtime.play(
        first_target, 0.0F, 1.0F, linear_spec(1000),
        AnimationTime::microseconds(2100));
    require(!runtime.contains(disposable) && !runtime.contains(first_target),
            "target teardown inside callback left stale animation state");
}

void test_stale_operations_and_diagnostics_conservation() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(4, 1, 1);
    RecordingSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar, AnimationDirtyDomain::material);
    const auto id = runtime.play(target, 0.0F, 1.0F, linear_spec(100), {});
    static_cast<void>(runtime.finish(id));
    require(!runtime.finish(id) && !runtime.cancel(id, {}),
            "stale animation operation was accepted");
    const auto& diagnostics = runtime.diagnostics();
    require(diagnostics.created
                == diagnostics.completed + diagnostics.canceled + diagnostics.active,
            "animation lifecycle diagnostics do not conserve created records");
    require(diagnostics.stale_operations >= 2
                && diagnostics.active == runtime.size(),
            "stale or active diagnostics are incorrect");
}

struct ThrowingSink final : ryn::animation::AnimationTargetSink {
    void apply(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId,
        const ryn::animation::AnimationValue&,
        ryn::animation::AnimationDirtyDomain) override {
        if (throw_apply) {
            throw_apply = false;
            throw std::runtime_error("apply failure");
        }
        ++successful_applies;
    }

    void completed(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId) override {
        if (throw_completion) {
            throw std::runtime_error("completion failure");
        }
    }

    bool throw_apply{false};
    bool throw_completion{false};
    int successful_applies{0};
};

void test_callback_exception_cleanup() {
    using namespace ryn::animation;
    AnimationRuntime runtime;
    runtime.reserve(2, 1, 1);
    ThrowingSink sink;
    const auto scope = runtime.create_scope();
    const auto target = runtime.register_target(
        scope, sink, AnimationValueKind::scalar, AnimationDirtyDomain::material);

    sink.throw_apply = true;
    require_throws<std::runtime_error>(
        [&] {
            static_cast<void>(runtime.play(
                target, 0.0F, 1.0F, linear_spec(1000), {}));
        },
        "apply callback exception was swallowed");
    require(runtime.size() == 0,
            "failed initial apply left an active animation record");

    sink.throw_completion = true;
    const auto id = runtime.play(target, 0.0F, 1.0F, linear_spec(1000), {});
    require_throws<std::runtime_error>(
        [&] { static_cast<void>(runtime.finish(id)); },
        "completion callback exception was swallowed");
    require(!runtime.contains(id) && runtime.size() == 0,
            "failed completion callback left an active animation record");
}

} // namespace

int main() {
    try {
        test_scope_target_generation_and_dirty_validation();
        test_owner_thread_guard();
        test_delay_cancel_finish_and_retarget();
        test_callback_reentrancy_and_tick_snapshot();
        test_stale_operations_and_diagnostics_conservation();
        test_callback_exception_cleanup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
