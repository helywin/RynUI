#include "input/pointer_router.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Fixture final {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components{nodes};
    ryn::input::InteractionRegistry registry{components, nodes};
    ryn::input::HitTestSnapshot hit_test{registry, nodes};
    ryn::input::PointerRouter router{registry, hit_test};
    ryn::runtime::ComponentId root_component;
    ryn::runtime::ComponentId target_component;
    ryn::input::InteractionId root;
    ryn::input::InteractionId target;

    Fixture() {
        components.mount(ryn::Content{[&] {
            auto& build = ryn::runtime::require_component_build_context();
            root_component = build.mount_component<TestState>();
            root = registry.create({
                root_component,
                build.root(root_component),
                std::nullopt,
                true,
                false,
                {},
            });
            build.on_resource_cleanup(root_component, [this, interaction = root] {
                router.cancel_interaction(interaction);
                static_cast<void>(registry.remove(interaction));
            });
            build.mount_slot(root_component, Children{[&] {
                auto& child_build = ryn::runtime::require_component_build_context();
                target_component = child_build.mount_component<TestState>();
                target = registry.create({
                    target_component,
                    child_build.root(target_component),
                    root,
                    true,
                    true,
                    {},
                });
                child_build.on_resource_cleanup(
                    target_component,
                    [this, interaction = target] {
                        router.cancel_interaction(interaction);
                        static_cast<void>(registry.remove(interaction));
                    });
            }});
        }});
        commit(components.root(root_component), {0.0F, 0.0F, 100.0F, 100.0F});
        commit(components.root(target_component), {10.0F, 10.0F, 80.0F, 80.0F});
        rebuild();
        router.reserve(1, 4);
    }

    ~Fixture() {
        components.dispose();
    }

    void commit(ryn::runtime::NodeId id, ryn::runtime::Rect bounds) {
        auto& node = nodes.require(id);
        node.bounds = bounds;
        node.place_generation = 1;
    }

    void rebuild() {
        const std::array entries{
            ryn::input::HitTestPaintEntry{root, std::nullopt},
            ryn::input::HitTestPaintEntry{target, std::nullopt},
        };
        hit_test.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});
    }
};

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(),
        action,
        button,
        20.0F,
        20.0F,
    };
}

void test_self_destroy_keeps_handler_snapshot_and_skips_stale_route() {
    Fixture fixture;
    std::vector<std::string> log;
    ryn::input::InteractionHandlers root_handlers;
    root_handlers.capture = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() == ryn::input::PointerEventKind::down) {
            log.push_back("root-capture");
        }
    };
    root_handlers.bubble = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() == ryn::input::PointerEventKind::down) {
            log.push_back("root-bubble");
        }
    };
    fixture.registry.set_handlers(fixture.root, std::move(root_handlers));
    ryn::input::InteractionHandlers target_handlers;
    target_handlers.target = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() != ryn::input::PointerEventKind::down) {
            return;
        }
        log.push_back("target");
        require(event.capture_pointer(), "self-destroy setup capture failed");
        require(fixture.components.destroy(fixture.target_component),
                "self-destroy callback failed");
        log.push_back("target-returned");
    };
    fixture.registry.set_handlers(fixture.target, std::move(target_handlers));

    fixture.router.dispatch(pointer_event(
        ryn::input::PointerAction::down,
        ryn::input::PointerButton::primary));
    require(log == std::vector<std::string>({
                "root-capture", "target", "target-returned", "root-bubble"}),
            "self-destroy changed the safe remaining route");
    require(!fixture.registry.contains(fixture.target),
            "self-destroy retained interaction identity");
    const auto state = fixture.router.state(ryn::input::PointerIdentity::mouse());
    require(!state->capture.has_value()
                && !state->press_origin.has_value()
                && !state->primary_down,
            "self-destroy retained pointer state");
}

void test_ancestor_destroy_skips_stale_descendants() {
    Fixture fixture;
    int target_calls = 0;
    ryn::input::InteractionHandlers root_handlers;
    root_handlers.capture = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() == ryn::input::PointerEventKind::move) {
            require(fixture.components.destroy(fixture.root_component),
                    "ancestor destroy callback failed");
        }
    };
    fixture.registry.set_handlers(fixture.root, std::move(root_handlers));
    ryn::input::InteractionHandlers target_handlers;
    target_handlers.target = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() == ryn::input::PointerEventKind::move) {
            ++target_calls;
        }
    };
    fixture.registry.set_handlers(fixture.target, std::move(target_handlers));

    fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
    require(target_calls == 0,
            "destroyed descendant handler executed from a stale route");
    require(fixture.registry.size() == 0 && fixture.nodes.size() == 0,
            "ancestor destroy retained interaction/Node state");
    require(fixture.router.diagnostics().stale_skips > 0,
            "ancestor destroy stale route was not diagnosed");
}

void test_slot_reuse_during_callback_does_not_run_replacement() {
    Fixture fixture;
    int replacement_calls = 0;
    ryn::input::InteractionId replacement;
    ryn::input::InteractionHandlers target_handlers;
    target_handlers.target = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() != ryn::input::PointerEventKind::move) {
            return;
        }
        require(fixture.registry.remove(fixture.target),
                "slot reuse callback remove failed");
        replacement = fixture.registry.create({
            fixture.target_component,
            fixture.components.root(fixture.target_component),
            fixture.root,
            true,
            true,
            {},
        });
        ryn::input::InteractionHandlers replacement_handlers;
        replacement_handlers.target = [&](ryn::input::PointerDispatchContext&) {
            ++replacement_calls;
        };
        fixture.registry.set_handlers(replacement, std::move(replacement_handlers));
    };
    fixture.registry.set_handlers(fixture.target, std::move(target_handlers));

    fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
    require(replacement.index == fixture.target.index
                && replacement.generation != fixture.target.generation,
            "callback did not reuse the interaction slot generation");
    require(replacement_calls == 0,
            "replacement handler ran from the stale route snapshot");
}

void test_disable_scope_dispose_exception_and_reentry_cleanup() {
    {
        Fixture fixture;
        int cancel_calls = 0;
        ryn::input::InteractionHandlers handlers;
        handlers.target = [&](ryn::input::PointerDispatchContext& event) {
            if (event.kind() == ryn::input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
            } else if (event.kind() == ryn::input::PointerEventKind::cancel) {
                ++cancel_calls;
            }
        };
        fixture.registry.set_handlers(fixture.target, std::move(handlers));
        fixture.router.dispatch(pointer_event(
            ryn::input::PointerAction::down,
            ryn::input::PointerButton::primary));
        fixture.registry.set_eligible(fixture.target, false);
        fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
        require(cancel_calls == 1,
                "disabled captured interaction did not receive cancel");
        require(!fixture.router.state(ryn::input::PointerIdentity::mouse())
                    ->capture.has_value(),
                "disabled interaction retained capture");
    }
    {
        Fixture fixture;
        int cancel_calls = 0;
        ryn::input::InteractionHandlers handlers;
        handlers.target = [&](ryn::input::PointerDispatchContext& event) {
            if (event.kind() == ryn::input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
            } else if (event.kind() == ryn::input::PointerEventKind::cancel) {
                ++cancel_calls;
            }
        };
        fixture.registry.set_handlers(fixture.target, std::move(handlers));
        fixture.router.dispatch(pointer_event(
            ryn::input::PointerAction::down,
            ryn::input::PointerButton::primary));
        require(fixture.components.destroy(fixture.target_component),
                "Scope dispose setup destroy failed");
        require(cancel_calls == 1,
                "Scope/resource cleanup did not cancel capture before removal");
    }
    {
        Fixture fixture;
        ryn::input::InteractionHandlers handlers;
        handlers.target = [&](ryn::input::PointerDispatchContext& event) {
            if (event.kind() == ryn::input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
                throw std::runtime_error("callback failure");
            }
        };
        fixture.registry.set_handlers(fixture.target, std::move(handlers));
        bool observed = false;
        try {
            fixture.router.dispatch(pointer_event(
                ryn::input::PointerAction::down,
                ryn::input::PointerButton::primary));
        } catch (const std::runtime_error&) {
            observed = true;
        }
        const auto state = fixture.router.state(ryn::input::PointerIdentity::mouse());
        require(observed
                    && !state->capture.has_value()
                    && !state->press_origin.has_value()
                    && !state->primary_down,
                "callback exception retained primary pointer state");
    }
    {
        Fixture fixture;
        bool rejected = false;
        ryn::input::InteractionHandlers handlers;
        handlers.target = [&](ryn::input::PointerDispatchContext& event) {
            if (event.kind() != ryn::input::PointerEventKind::move) {
                return;
            }
            try {
                fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
            } catch (const std::logic_error&) {
                rejected = true;
            }
        };
        fixture.registry.set_handlers(fixture.target, std::move(handlers));
        fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
        require(rejected
                    && fixture.router.diagnostics().reentrant_rejections == 1,
                "reentrant pointer dispatch did not fail fast");
    }
}

void test_wrong_thread_dispatch_is_rejected_without_state_change() {
    Fixture fixture;
    std::atomic<bool> rejected{false};
    std::thread worker([&] {
        try {
            fixture.router.dispatch(pointer_event(ryn::input::PointerAction::move));
        } catch (const std::logic_error&) {
            rejected.store(true, std::memory_order_relaxed);
        }
    });
    worker.join();

    require(rejected.load(std::memory_order_relaxed),
            "wrong-thread pointer dispatch did not fail fast");
    require(fixture.router.pointer_count() == 0
                && fixture.router.diagnostics().input_events == 0,
            "wrong-thread pointer dispatch mutated state");
}

} // namespace

int main() {
    try {
        test_self_destroy_keeps_handler_snapshot_and_skips_stale_route();
        test_ancestor_destroy_skips_stale_descendants();
        test_slot_reuse_during_callback_does_not_run_replacement();
        test_disable_scope_dispose_exception_and_reentry_cleanup();
        test_wrong_thread_dispatch_is_rejected_without_state_change();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
