#include <ryn/reactive.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_scope_cleanup_and_notification_stop() {
    ryn::Signal<int> value{0};
    int effect_runs = 0;
    int cleanup_runs = 0;
    ryn::Scope scope;
    const auto handle = ryn::effect(scope, [&] {
        static_cast<void>(value.get());
        ++effect_runs;
    });
    scope.on_cleanup([&] { ++cleanup_runs; });

    require(effect_runs == 1, "effect did not run initially");
    require(handle.active(), "effect handle is inactive before disposal");
    value.set(1);
    require(effect_runs == 2, "live effect did not receive notification");

    scope.dispose();
    scope.dispose();
    require(cleanup_runs == 1, "scope cleanup did not run exactly once");
    require(!handle.active(), "effect remained active after scope disposal");
    value.set(2);
    require(effect_runs == 2, "disposed effect received notification");
}

void test_disposed_scope_rejects_new_effects() {
    int effect_runs = 0;
    ryn::Scope scope;
    scope.dispose();

    const auto handle = ryn::effect(scope, [&] { ++effect_runs; });

    require(effect_runs == 0, "disposed scope executed a new effect");
    require(!handle.active(), "disposed scope retained a new effect");
}

void test_effect_observes_stable_batch() {
    ryn::Signal<int> left{1};
    ryn::Signal<int> right{2};
    ryn::Memo<int> sum([&] { return left.get() + right.get(); });
    std::vector<int> observed;
    ryn::Scope scope;
    ryn::effect(scope, [&] { observed.push_back(sum.get()); });

    ryn::batch([&] {
        left.set(10);
        right.set(20);
    });

    require(observed == std::vector<int>({3, 30}), "effect observed partial batch state");
}

void test_effect_writes_use_later_epochs_without_reentry() {
    ryn::Signal<int> value{0};
    int runs = 0;
    int call_depth = 0;
    int maximum_depth = 0;
    ryn::Scope scope;

    ryn::effect(scope, [&] {
        ++runs;
        ++call_depth;
        maximum_depth = std::max(maximum_depth, call_depth);
        const int current = value.get();
        if (current < 3) {
            value.set(current + 1);
        }
        --call_depth;
    });

    require(value.get() == 3, "effect write rounds did not settle");
    require(runs == 4, "effect did not execute one controlled round per write");
    require(maximum_depth == 1, "effect synchronously reentered itself");
}

} // namespace

int main() {
    try {
        test_scope_cleanup_and_notification_stop();
        test_disposed_scope_rejects_new_effects();
        test_effect_observes_stable_batch();
        test_effect_writes_use_later_epochs_without_reentry();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
