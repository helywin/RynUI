#include <ryn/reactive.hpp>
#include "support/allocation_probe.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void destroyed_source_and_reentry() {
    ryn::Scope scope;
    ryn::Signal<int> trigger{0};
    auto source = std::make_unique<ryn::Signal<int>>(7);
    int value{};
    auto observer = ryn::effect(scope, [&] {
        static_cast<void>(trigger.get());
        value = source ? source->get() : -1;
    });
    check(value == 7, "source was not observed");
    ryn_test::allocation::begin(0);
    source.reset();
    const auto allocations = ryn_test::allocation::end();
    check(allocations == 0, "source cleanup allocated");
    trigger.set(1);
    check(value == -1 && observer.active(), "observer did not survive retired source");
    scope.dispose();
    check(!observer.active(), "retired source observer did not dispose");

    ryn::Scope reentrant_scope;
    source = std::make_unique<ryn::Signal<int>>(9);
    auto reentrant = ryn::effect(reentrant_scope, [&] {
        static_cast<void>(trigger.get());
        if (source) {
            value = source->get();
            source.reset();
        }
    });
    trigger.set(2);
    check(value == 9 && reentrant.active(), "source retired during observer could not rerun");
    reentrant_scope.dispose();
}

void queued_destruction() {
    ryn::Scope scope;
    auto source = std::make_unique<ryn::Signal<int>>(1);
    int runs{};
    const auto observer = ryn::effect(scope, [&] {
        ++runs;
        if (source) {
            static_cast<void>(source->get());
        }
    });
    ryn::batch([&] {
        source->set(2);
        source.reset();
    });
    check(runs == 2 && observer.active(), "queued observer retained destroyed source");
    scope.dispose();
}

void initial_callback_retires_scope() {
    ryn::Signal<int> trigger{0};
    auto effect_scope = std::make_unique<ryn::Scope>();
    int effect_runs{};
    const auto observer = ryn::effect(*effect_scope, [&] {
        ++effect_runs;
        static_cast<void>(trigger.get());
        effect_scope.reset();
    });
    check(!observer.active(), "initial effect retained a destroyed scope");

    auto binding_scope = std::make_unique<ryn::Scope>();
    int binding_runs{};
    const auto binding = ryn::connect_binding(*binding_scope, ryn::bind([&] {
        ++binding_runs;
        static_cast<void>(trigger.get());
        binding_scope.reset();
        return 7;
    }),
                                              [](int) {});
    check(!binding.active(), "initial binding retained a destroyed scope");
    trigger.set(1);
    check(effect_runs == 1 && binding_runs == 1, "initial callback retirement retained a subscription");
}
} // namespace

int main() {
    try {
        destroyed_source_and_reentry();
        queued_destruction();
        initial_callback_retires_scope();
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
