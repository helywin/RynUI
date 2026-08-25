#include <ryn/reactive.hpp>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_same_signal_is_queued_once() {
    ryn::Signal<int> value{0};
    int executions = 0;
    int observed = -1;
    const auto observer = ryn::detail::observe(
        ryn::detail::ObserverPhase::binding,
        [&] {
            ++executions;
            observed = value.get();
        });

    const auto epoch_before = ryn::detail::Scheduler::current().epoch();
    ryn::batch([&] {
        value.set(1);
        value.set(2);
        value.set(3);
        require(executions == 1, "observer ran before outer batch ended");
    });

    require(executions == 2, "observer was not deduplicated within batch");
    require(observed == 3, "observer did not see final batch value");
    require(
        ryn::detail::Scheduler::current().epoch() == epoch_before + 1,
        "single batch did not settle in one epoch");
    observer->deactivate();
}

void test_multiple_signals_stabilize_one_memo() {
    ryn::Signal<int> left{1};
    ryn::Signal<int> right{2};
    int memo_evaluations = 0;
    ryn::Memo<int> sum([&] {
        ++memo_evaluations;
        return left.get() + right.get();
    });

    int binding_executions = 0;
    int observed_sum = 0;
    const auto observer = ryn::detail::observe(
        ryn::detail::ObserverPhase::binding,
        [&] {
            ++binding_executions;
            observed_sum = sum.get();
        });

    ryn::batch([&] {
        left.set(10);
        right.set(20);
    });

    require(memo_evaluations == 2, "memo recomputed more than once for a batch");
    require(binding_executions == 2, "binding was not deduplicated for memo batch");
    require(observed_sum == 30, "binding observed a partial batch result");
    observer->deactivate();
}

void test_nested_batches_flush_at_outer_boundary() {
    ryn::Signal<int> value{0};
    int executions = 0;
    const auto observer = ryn::detail::observe(
        ryn::detail::ObserverPhase::binding,
        [&] {
            static_cast<void>(value.get());
            ++executions;
        });

    ryn::batch([&] {
        value.set(1);
        ryn::batch([&] { value.set(2); });
        require(executions == 1, "nested batch flushed before outer batch ended");
    });
    require(executions == 2, "outer batch did not flush once");
    observer->deactivate();
}

} // namespace

int main() {
    try {
        test_same_signal_is_queued_once();
        test_multiple_signals_stabilize_one_memo();
        test_nested_batches_flush_at_outer_boundary();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
