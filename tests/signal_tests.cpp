#include <ryn/reactive.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_precise_dependency_notification() {
    ryn::Signal<int> dependent{1};
    ryn::Signal<int> unrelated{10};
    int executions = 0;
    int observed = 0;

    const auto observer = ryn::detail::observe(ryn::detail::ObserverPhase::binding, [&] {
        ++executions;
        observed = dependent.get();
    });

    require(executions == 1, "observer did not execute initially");
    require(observed == 1, "observer read the wrong initial value");

    require(unrelated.set(11), "unrelated value did not change");
    require(executions == 1, "unrelated signal reran the observer");

    require(dependent.set(2), "dependent value did not change");
    require(executions == 2, "dependent signal did not rerun the observer");
    require(observed == 2, "observer did not read the changed value");

    require(!dependent.set(2), "equal-value write reported a change");
    require(executions == 2, "equal-value write propagated");

    observer->deactivate();
    require(dependent.set(3), "post-deactivation value did not change");
    require(executions == 2, "inactive observer received a notification");
}

void test_owner_thread_rejects_reads_and_writes_before_mutation() {
    ryn::Signal<int> signal{7};
    bool read_rejected = false;
    bool write_rejected = false;
    std::thread worker([&] {
        try {
            static_cast<void>(signal.get());
        } catch (const std::logic_error&) {
            read_rejected = true;
        }
        try {
            signal.set(9);
        } catch (const std::logic_error&) {
            write_rejected = true;
        }
    });
    worker.join();

    require(read_rejected && write_rejected, "non-owner Signal access did not fail fast");
    require(signal.get() == 7, "rejected non-owner Signal write changed the value");
}

void test_large_values_remain_owned_during_reentrant_notification() {
    struct Value final {
        std::array<int, 1024> data{};
        bool operator==(const Value&) const = default;
    };

    Value caller;
    caller.data[0] = 1;
    ryn::Signal<Value> signal{caller};
    caller.data[0] = 9;
    require(signal.get().data[0] == 1, "Signal construction borrowed its caller's large value");
    int notifications{};
    const auto observer = ryn::detail::observe(ryn::detail::ObserverPhase::binding, [&] {
        ++notifications;
        if (signal.get().data[0] == 2) {
            auto next = signal.get();
            next.data[0] = 3;
            signal.set(std::move(next));
        }
    });
    caller.data[0] = 2;
    require(signal.set(caller), "large value update did not publish");
    caller.data[0] = 8;
    require(signal.get().data[0] == 3 && notifications == 3,
            "large value reentry lost notification or aliased the caller");
    require(!signal.set(signal.get()) && notifications == 3, "large equal value propagated or invalidated itself");
    observer->deactivate();
}

} // namespace

int main() {
    try {
        test_precise_dependency_notification();
        test_owner_thread_rejects_reads_and_writes_before_mutation();
        test_large_values_remain_owned_during_reentrant_notification();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
