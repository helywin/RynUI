#include "runtime/prop_connection.hpp"

#include <ryn/prop.hpp>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct FieldCounters final {
    int value{};
    int applies{};
    int dirty_marks{};
    int frame_requests{};

    void apply(int next_value) {
        value = next_value;
        ++applies;
        ++dirty_marks;
        ++frame_requests;
    }
};

struct Bucket final {
    int value{};
};

struct BucketEqual final {
    bool operator()(const Bucket& left, const Bucket& right) const noexcept {
        return left.value / 10 == right.value / 10;
    }
};

ryn::Prop<int> make_temporary_binding_prop(const ryn::Signal<int>& source) {
    return ryn::Prop<int>{ryn::bind([source] {
        return source.get() * 2;
    })};
}

void test_static_prop_applies_once_without_observer() {
    ryn::Scope scope;
    FieldCounters counters;

    const auto connection = ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{7},
        [&](int value) { counters.apply(value); });

    require(!connection.active(), "static Prop unexpectedly created an Observer");
    require(counters.value == 7 && counters.applies == 1,
            "static Prop did not apply exactly once");
}

void test_signal_and_binding_share_the_field_path() {
    ryn::Signal<int> signal_source{3};
    ryn::Signal<int> binding_source{40};
    ryn::Scope scope;
    FieldCounters signal_counters;
    FieldCounters binding_counters;

    const auto signal_connection = ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{signal_source},
        [&](int value) { signal_counters.apply(value); });
    const auto binding_connection = ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{ryn::bind([binding_source] {
            return binding_source.get() / 10;
        })},
        [&](int value) { binding_counters.apply(value); });

    require(signal_connection.active() && binding_connection.active(),
            "reactive Prop did not create a live binding-phase Observer");
    require(signal_counters.value == 3 && signal_counters.applies == 1,
            "Signal Prop did not use the field apply path initially");
    require(binding_counters.value == 4 && binding_counters.applies == 1,
            "Binding Prop did not use the field apply path initially");

    signal_source.set(5);
    binding_source.set(41);
    require(signal_counters.value == 5 && signal_counters.applies == 2,
            "Signal Prop did not update the field path");
    require(binding_counters.value == 4 && binding_counters.applies == 1,
            "equal Binding result expanded field invalidation");
    require(binding_counters.dirty_marks == 1
                && binding_counters.frame_requests == 1,
            "equal Binding result marked Dirty or requested a frame");

    binding_source.set(50);
    require(binding_counters.value == 5 && binding_counters.applies == 2,
            "changed Binding result did not update the field path");
}

void test_custom_signal_equality_and_custom_field_equality() {
    ryn::Signal<Bucket, BucketEqual> source{Bucket{10}, BucketEqual{}};
    ryn::Scope scope;
    int applies = 0;
    int observed = 0;

    const auto connection = ryn::detail::connect_prop(
        scope,
        ryn::Prop<Bucket>{source},
        [&](Bucket value) {
            observed = value.value;
            ++applies;
        },
        BucketEqual{});

    require(connection.active() && observed == 10 && applies == 1,
            "custom-equality Signal Prop did not apply initially");
    require(!source.set(Bucket{19}),
            "custom Signal equality accepted an equivalent value");
    require(applies == 1, "equivalent custom Signal value propagated");
    require(source.set(Bucket{20}),
            "custom Signal equality rejected a changed value");
    require(observed == 20 && applies == 2,
            "custom-equality Signal Prop did not propagate a changed value");
}

void test_temporary_prop_and_binding_own_their_sources() {
    ryn::Signal<int> source{4};
    ryn::Scope scope;
    FieldCounters counters;

    const auto connection = ryn::detail::connect_prop(
        scope,
        make_temporary_binding_prop(source),
        [&](int value) { counters.apply(value); });

    require(connection.active() && counters.value == 8,
            "temporary Binding Prop was not retained after mount");
    source.set(6);
    require(counters.value == 12 && counters.applies == 2,
            "temporary Props or Binding lifetime was borrowed");
}

void test_scope_disposal_stops_updates_and_queued_work() {
    ryn::Signal<int> source{1};
    ryn::Scope scope;
    FieldCounters counters;
    const auto connection = ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{source},
        [&](int value) { counters.apply(value); });

    require(connection.active() && counters.applies == 1,
            "scoped Prop connection did not start");
    ryn::batch([&] {
        source.set(2);
        scope.dispose();
    });
    require(!connection.active(), "disposed Prop connection remained active");
    require(counters.value == 1 && counters.applies == 1,
            "queued Prop work ran after Scope disposal");

    source.set(3);
    require(counters.applies == 1
                && counters.dirty_marks == 1
                && counters.frame_requests == 1,
            "destroyed Prop field received a later update");
}

void test_one_prop_does_not_update_a_sibling_field() {
    ryn::Signal<int> first_source{1};
    ryn::Signal<int> second_source{2};
    ryn::Scope scope;
    FieldCounters first;
    FieldCounters second;
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{first_source},
        [&](int value) { first.apply(value); }));
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        ryn::Prop<int>{second_source},
        [&](int value) { second.apply(value); }));

    first_source.set(9);
    require(first.value == 9 && first.applies == 2,
            "target Prop did not update");
    require(second.value == 2 && second.applies == 1,
            "unrelated sibling Prop update path executed");
}

} // namespace

int main() {
    try {
        test_static_prop_applies_once_without_observer();
        test_signal_and_binding_share_the_field_path();
        test_custom_signal_equality_and_custom_field_equality();
        test_temporary_prop_and_binding_own_their_sources();
        test_scope_disposal_stops_updates_and_queued_work();
        test_one_prop_does_not_update_a_sibling_field();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
