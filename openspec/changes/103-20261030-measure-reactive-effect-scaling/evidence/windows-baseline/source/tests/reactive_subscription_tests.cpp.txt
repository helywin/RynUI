#include <ryn/reactive.hpp>

#include "support/allocation_probe.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void subscription_order() {
    ryn::Signal<int> shared{0};
    ryn::Signal<int> rerun_b{0};
    std::vector<char> delivered;
    ryn::Scope scope;
    ryn::effect(scope, [&] {
        static_cast<void>(shared.get());
        delivered.push_back('A');
    });
    ryn::effect(scope, [&] {
        static_cast<void>(rerun_b.get());
        static_cast<void>(shared.get());
        delivered.push_back('B');
    });
    ryn::effect(scope, [&] {
        static_cast<void>(shared.get());
        delivered.push_back('C');
    });
    require(delivered == std::vector<char>({'A', 'B', 'C'}), "initial observer order differs");

    delivered.clear();
    rerun_b.set(1);
    require(delivered == std::vector<char>({'B'}), "private trigger reran a foreign observer");
    delivered.clear();
    shared.set(1);
    require(delivered == std::vector<char>({'A', 'C', 'B'}), "resubscribed B did not move to the source tail");

    delivered.clear();
    ryn::batch([&] {
        shared.set(2);
        shared.set(3);
    });
    require(delivered == std::vector<char>({'A', 'C', 'B'}), "batch changed delivery order or duplicated a callback");
    scope.dispose();
    delivered.clear();
    shared.set(4);
    rerun_b.set(2);
    require(delivered.empty(), "disposed ordered observers still ran");
}

void read_write_order() {
    enum class Write { none, before_read, after_read };
    ryn::Signal<int> trigger{0};
    ryn::Signal<int> value{0};
    Write next_write{Write::none};
    int runs{};
    int depth{};
    int maximum_depth{};
    std::vector<int> observed;
    ryn::Scope scope;
    ryn::effect(scope, [&] {
        ++runs;
        ++depth;
        maximum_depth = std::max(maximum_depth, depth);
        static_cast<void>(trigger.get());
        const auto write = next_write;
        next_write = Write::none;
        if (write == Write::before_read) {
            value.set(1);
        }
        const int current = value.get();
        observed.push_back(current);
        if (write == Write::after_read) {
            value.set(current + 1);
        }
        --depth;
    });
    require(runs == 1 && observed == std::vector<int>({0}), "initial read/write fixture differs");

    auto& scheduler = ryn::detail::Scheduler::current();
    const auto before_epoch = scheduler.epoch();
    next_write = Write::before_read;
    trigger.set(1);
    require(runs == 2 && observed == std::vector<int>({0, 1}), "write before first read retriggered the observer");
    require(scheduler.epoch() == before_epoch + 1, "write before first read added an unnecessary epoch");

    const auto after_epoch = scheduler.epoch();
    next_write = Write::after_read;
    trigger.set(2);
    require(runs == 4 && observed == std::vector<int>({0, 1, 1, 2}), "write after read lost its later observer run");
    require(scheduler.epoch() == after_epoch + 2 && maximum_depth == 1 && depth == 0,
            "write after read reentered or did not use a later epoch");
}

void dynamic_disposal() {
    ryn::Signal<bool> choose_left{true};
    ryn::Signal<int> trigger{0};
    auto left = std::make_unique<ryn::Signal<int>>(10);
    auto right = std::make_unique<ryn::Signal<int>>(20);
    int observed{};
    int runs{};
    ryn::Scope scope;
    const auto handle = ryn::effect(scope, [&] {
        ++runs;
        static_cast<void>(trigger.get());
        const auto& selected = choose_left.get() ? left : right;
        observed = selected ? selected->get() : -1;
    });
    require(runs == 1 && observed == 10, "dynamic initial value differs");
    left->set(11);
    require(runs == 2 && observed == 11, "active source did not notify");
    choose_left.set(false);
    require(runs == 3 && observed == 20, "dynamic source did not switch");
    left->set(12);
    require(runs == 3 && observed == 20, "old dynamic dependency still notified");
    right->set(21);
    require(runs == 4 && observed == 21, "new dynamic dependency did not notify");

    right.reset();
    trigger.set(1);
    require(runs == 5 && observed == -1 && handle.active(), "destroyed active source left a borrowed dependency");
    right = std::make_unique<ryn::Signal<int>>(30);
    trigger.set(2);
    right->set(31);
    require(runs == 7 && observed == 31, "replacement source was not tracked");
    scope.dispose();
    scope.dispose();
    require(!handle.active(), "scope disposal retained an active handle");
    left->set(13);
    right->set(32);
    choose_left.set(true);
    trigger.set(3);
    require(runs == 7 && observed == 31, "disposed dynamic observer ran");
}

void source_and_scope_lifetime() {
    {
        auto source = std::make_unique<ryn::Signal<int>>(1);
        int observed{};
        int runs{};
        ryn::Scope scope;
        const auto handle = ryn::effect(scope, [&] {
            ++runs;
            observed = source ? source->get() : -1;
        });
        ryn::batch([&] {
            source->set(2);
            source.reset();
        });
        require(runs == 2 && observed == -1 && handle.active(), "queued observer kept a destroyed source");
    }
    {
        ryn::Signal<int> trigger{0};
        auto source = std::make_unique<ryn::Signal<int>>(7);
        bool destroy_on_read{};
        int observed{};
        int runs{};
        ryn::Scope scope;
        const auto handle = ryn::effect(scope, [&] {
            ++runs;
            static_cast<void>(trigger.get());
            if (source) {
                observed = source->get();
                if (destroy_on_read) {
                    source.reset();
                }
            }
        });
        destroy_on_read = true;
        trigger.set(1);
        trigger.set(2);
        require(runs == 3 && observed == 7 && handle.active(), "source destruction inside callback broke tracking");
    }
    {
        ryn::Signal<int> trigger{0};
        ryn::Signal<int> read_after_dispose{3};
        bool dispose_in_callback{};
        int runs{};
        int observed{};
        ryn::Scope scope;
        const auto handle = ryn::effect(scope, [&] {
            ++runs;
            static_cast<void>(trigger.get());
            if (dispose_in_callback) {
                scope.dispose();
            }
            observed = read_after_dispose.get();
        });
        dispose_in_callback = true;
        trigger.set(1);
        require(runs == 2 && observed == 3 && !handle.active(), "callback disposal did not deactivate the observer");
        read_after_dispose.set(4);
        trigger.set(2);
        require(runs == 2 && observed == 3, "read after callback disposal resurrected notifications");
    }
}

struct InitialFailure final {
    int payload{};
};

void initial_exception() {
    ryn::Signal<int> first{1};
    ryn::Signal<int> second{2};
    ryn::Scope scope;
    int failed_runs{};
    bool original_exception{};
    try {
        ryn::effect(scope, [&] {
            ++failed_runs;
            static_cast<void>(first.get());
            static_cast<void>(second.get());
            throw InitialFailure{73};
        });
    } catch (const InitialFailure& error) {
        original_exception = error.payload == 73;
    }
    require(original_exception && failed_runs == 1, "initial observer exception was changed or swallowed");
    int healthy_runs{};
    int sum{};
    const auto healthy = ryn::effect(scope, [&] {
        ++healthy_runs;
        sum = first.get() + second.get();
    });
    first.set(3);
    second.set(4);
    require(failed_runs == 1 && healthy_runs == 3 && sum == 7 && healthy.active(),
            "initial exception leaked a subscription or poisoned later observers");
    scope.dispose();
    first.set(5);
    require(healthy_runs == 3, "healthy observer did not dispose after failed mount");
}

void subscription_growth_failure() {
    std::size_t failures{};
    for (std::size_t population = 1; population <= 16; ++population) {
        for (std::size_t fail = 0; fail <= 8; ++fail) {
            ryn::Signal<int> first{1};
            ryn::Signal<int> second{2};
            std::vector<int> foreign_runs(population);
            std::vector<int> foreign_values(population);
            ryn::Scope foreign_scope;
            for (std::size_t index = 0; index < population; ++index) {
                ryn::effect(foreign_scope, [&, index] {
                    ++foreign_runs[index];
                    foreign_values[index] = first.get() + second.get();
                });
            }
            const auto verify_foreign = [&](int expected_runs, int expected_sum) {
                for (std::size_t index = 0; index < population; ++index) {
                    require(foreign_runs[index] == expected_runs && foreign_values[index] == expected_sum,
                            "subscription growth failure damaged a foreign observer");
                }
            };
            verify_foreign(1, 3);
            int original_runs{};
            // Construct the observer before injection. Its callback performs only get():
            // no notify, schedule, queue growth, exception payload allocation, or set().
            // Prepopulation makes subscriber storage grow beyond tiny MSVC iterator/proxy
            // allocations. Fail only requests >= 64 bytes; this is not arbitrary OOM coverage.
            auto original = ryn::detail::observe(
                ryn::detail::ObserverPhase::effect,
                [&] {
                    ++original_runs;
                    static_cast<void>(first.get());
                    static_cast<void>(second.get());
                },
                false);
            bool failed{};
            ryn_test::allocation::begin(fail, 64);
            try {
                original->run();
            } catch (const std::bad_alloc&) {
                failed = true;
            } catch (...) {
                ryn_test::allocation::end();
                throw;
            }
            ryn_test::allocation::end();
            failures += failed ? 1 : 0;
            require(original_runs == 1, "subscription growth unexpectedly reentered the callback");
            original.reset();
            verify_foreign(1, 3);

            int healthy_runs{};
            int sum{};
            ryn::Scope healthy_scope;
            ryn::effect(healthy_scope, [&] {
                ++healthy_runs;
                sum = first.get() + second.get();
            });
            first.set(3);
            second.set(4);
            require(original_runs == 1 && healthy_runs == 3 && sum == 7,
                    "subscription growth failure left an observer or corrupted a source");
            verify_foreign(3, 7);
            healthy_scope.dispose();
            first.set(5);
            second.set(6);
            require(healthy_runs == 3, "post-failure healthy observer leaked after disposal");
            verify_foreign(5, 11);
            foreign_scope.dispose();
            foreign_scope.dispose();
            first.set(7);
            second.set(8);
            verify_foreign(5, 11);
        }
    }
    require(failures > 0, "subscription growth probe did not inject an allocation failure");
    std::cout << "subscription_growth_bad_alloc=" << failures << '\n';
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    // Keep a failed headless probe noninteractive without suppressing its exit failure.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
    try {
        require(argc <= 2, "expected at most one case name");
        const std::string_view selected = argc == 2 ? argv[1] : "all";
        bool matched{};
        const auto run = [&](std::string_view name, auto test) {
            if (selected == "all" || selected == name) {
                test();
                matched = true;
                std::cout << "PASS " << name << '\n';
            }
        };
        run("order", subscription_order);
        run("read_write", read_write_order);
        run("dynamic", dynamic_disposal);
        run("lifetime", source_and_scope_lifetime);
        run("initial_exception", initial_exception);
        run("allocation", subscription_growth_failure);
        require(matched, "unknown case name");
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    } catch (...) {
        ryn_test::allocation::end();
        std::cerr << "unexpected non-standard exception\n";
        return 1;
    }
    return 0;
}
