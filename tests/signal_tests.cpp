#include <ryn/reactive.hpp>

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

    const auto observer = ryn::detail::observe(
        ryn::detail::ObserverPhase::binding,
        [&] {
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

} // namespace

int main() {
    try {
        test_precise_dependency_notification();
        test_owner_thread_rejects_reads_and_writes_before_mutation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
