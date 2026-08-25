#include <ryn/reactive.hpp>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_cache_and_dynamic_dependencies() {
    ryn::Signal<bool> use_left{true};
    ryn::Signal<int> left{10};
    ryn::Signal<int> right{20};
    int evaluations = 0;

    ryn::Memo<int> selected([&] {
        ++evaluations;
        return use_left.get() ? left.get() : right.get();
    });

    require(evaluations == 1, "memo did not compute initially");
    require(selected.get() == 10, "memo initial value differs");
    require(selected.get() == 10, "memo repeated value differs");
    require(evaluations == 1, "valid memo recomputed on repeated read");

    right.set(21);
    require(evaluations == 1, "inactive memo dependency triggered recompute");

    int downstream_runs = 0;
    int downstream_value = 0;
    const auto downstream = ryn::detail::observe(
        ryn::detail::ObserverPhase::binding,
        [&] {
            ++downstream_runs;
            downstream_value = selected.get();
        });
    require(downstream_runs == 1, "memo downstream observer did not start");

    left.set(11);
    require(evaluations == 2, "active memo dependency did not recompute");
    require(downstream_runs == 2, "memo change did not notify downstream");
    require(downstream_value == 11, "downstream saw stale memo value");

    use_left.set(false);
    require(evaluations == 3, "memo selector change did not recompute");
    require(selected.get() == 21, "memo did not switch to the new dependency");

    left.set(12);
    require(evaluations == 3, "old memo dependency remained subscribed");

    right.set(22);
    require(evaluations == 4, "new memo dependency did not invalidate memo");
    require(selected.get() == 22, "memo cache did not receive new dependency value");

    downstream->deactivate();
}

} // namespace

int main() {
    try {
        test_cache_and_dynamic_dependencies();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
