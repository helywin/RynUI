#include "input/focus_manager.hpp"
#include "input/pointer_router.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

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
    ryn::runtime::FrameRequestState frames;
    ryn::input::FocusManager focus{registry, &frames};
    ryn::input::PointerRouter pointer{registry, hit_test, &frames, &focus};
    ryn::runtime::ComponentId root_component;
    ryn::runtime::ComponentId target_component;
    ryn::input::InteractionId root;
    ryn::input::InteractionId target;
    int pointer_cancels{0};
    int keyboard_activations{0};

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
                pointer.cancel_interaction(interaction);
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
                        pointer.cancel_interaction(interaction);
                        static_cast<void>(registry.remove(interaction));
                    });
            }});
        }});
        commit(components.root(root_component), {0.0F, 0.0F, 100.0F, 100.0F});
        commit(components.root(target_component), {10.0F, 10.0F, 80.0F, 80.0F});
        const std::array entries{
            ryn::input::HitTestPaintEntry{root, std::nullopt},
            ryn::input::HitTestPaintEntry{target, std::nullopt},
        };
        hit_test.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});
        focus.reserve(2);
        pointer.reserve(1, 4);

        ryn::input::InteractionHandlers pointer_handlers;
        pointer_handlers.target = [&](ryn::input::PointerDispatchContext& event) {
            if (event.kind() == ryn::input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
            } else if (event.kind() == ryn::input::PointerEventKind::cancel) {
                ++pointer_cancels;
            }
        };
        registry.set_handlers(target, std::move(pointer_handlers));
        ryn::input::FocusHandlers focus_handlers;
        focus_handlers.activate = [&] { ++keyboard_activations; };
        registry.set_focus_handlers(target, std::move(focus_handlers));
    }

    ~Fixture() {
        components.dispose();
    }

    void commit(ryn::runtime::NodeId node, ryn::runtime::Rect bounds) {
        auto& record = nodes.require(node);
        record.bounds = bounds;
        record.place_generation = 1;
    }
};

ryn::input::PointerInputEvent pointer_down() {
    return {
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::down,
        ryn::input::PointerButton::primary,
        20.0F,
        20.0F,
    };
}

ryn::input::KeyboardInputEvent space_down() {
    return {
        ryn::input::Key::space,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    };
}

void test_scope_cleanup_cancels_pending_space_activation() {
    Fixture fixture;
    fixture.focus.request_focus(
        fixture.target, ryn::input::FocusModality::keyboard);
    fixture.focus.dispatch(space_down());
    require(fixture.focus.state().keyboard_pressed,
            "Scope cleanup setup did not enter keyboard pressed state");
    require(fixture.components.destroy(fixture.target_component),
            "keyboard pressed target destroy failed");
    fixture.focus.dispatch({
        ryn::input::Key::space,
        ryn::input::KeyAction::up,
        ryn::input::KeyModifier::none,
        false,
    });
    require(!fixture.focus.state().focused.has_value()
                && !fixture.focus.state().keyboard_pressed
                && fixture.keyboard_activations == 0,
            "destroyed Space target retained state or activated");
}

void test_scope_cleanup_releases_pointer_and_focus_before_slot_reuse() {
    Fixture fixture;
    fixture.pointer.dispatch(pointer_down());
    require(fixture.focus.state().focused == fixture.target
                && fixture.pointer.state(ryn::input::PointerIdentity::mouse())
                    ->capture == fixture.target,
            "shared lifecycle setup did not establish focus and capture");

    require(fixture.components.destroy(fixture.target_component),
            "focused/captured target destroy failed");
    require(!fixture.focus.state().focused.has_value()
                && !fixture.pointer.state(ryn::input::PointerIdentity::mouse())
                    ->capture.has_value()
                && fixture.pointer_cancels == 1,
            "Scope cleanup retained focus/capture or skipped pointer cancel");
    require(!fixture.registry.contains(fixture.target),
            "Scope cleanup retained interaction registration");

    const auto replacement = fixture.registry.create({
        fixture.root_component,
        fixture.components.root(fixture.root_component),
        fixture.root,
        true,
        true,
        {},
    });
    require(replacement.index == fixture.target.index
                && replacement.generation != fixture.target.generation
                && !fixture.focus.state().focused.has_value(),
            "reused interaction slot inherited stale focus");
    fixture.focus.cancel_interaction(replacement);
    static_cast<void>(fixture.registry.remove(replacement));
}

void test_wrong_thread_reentry_and_callback_exception_fail_safely() {
    {
        Fixture fixture;
        std::atomic<bool> rejected{false};
        std::thread worker([&] {
            try {
                fixture.focus.dispatch(space_down());
            } catch (const std::logic_error&) {
                rejected.store(true, std::memory_order_relaxed);
            }
        });
        worker.join();
        require(rejected.load(std::memory_order_relaxed)
                    && fixture.focus.diagnostics().keyboard_events == 0,
                "wrong-thread keyboard dispatch mutated FocusManager");
    }
    {
        Fixture fixture;
        bool reentry_rejected = false;
        ryn::input::FocusHandlers handlers;
        handlers.state_changed = [&](ryn::input::FocusPresentation state) {
            if (!state.focused) {
                return;
            }
            try {
                fixture.focus.dispatch({
                    ryn::input::Key::tab,
                    ryn::input::KeyAction::down,
                    ryn::input::KeyModifier::none,
                    false,
                });
            } catch (const std::logic_error&) {
                reentry_rejected = true;
            }
        };
        handlers.activate = [] {};
        fixture.registry.set_focus_handlers(fixture.target, std::move(handlers));
        fixture.focus.request_focus(
            fixture.target, ryn::input::FocusModality::keyboard);
        require(reentry_rejected
                    && fixture.focus.diagnostics().reentrant_rejections == 1,
                "reentrant FocusManager dispatch did not fail fast");
    }
    {
        Fixture fixture;
        ryn::input::FocusHandlers handlers;
        handlers.state_changed = [](ryn::input::FocusPresentation state) {
            if (state.keyboard_pressed) {
                throw std::runtime_error("focus callback failure");
            }
        };
        handlers.activate = [] {};
        fixture.registry.set_focus_handlers(fixture.target, std::move(handlers));
        fixture.focus.request_focus(
            fixture.target, ryn::input::FocusModality::keyboard);
        bool observed = false;
        try {
            fixture.focus.dispatch(space_down());
        } catch (const std::runtime_error&) {
            observed = true;
        }
        require(observed && !fixture.focus.state().keyboard_pressed,
                "focus callback exception retained keyboard pressed state");
    }
}

} // namespace

int main() {
    try {
        test_scope_cleanup_releases_pointer_and_focus_before_slot_reuse();
        test_scope_cleanup_cancels_pending_space_activation();
        test_wrong_thread_reentry_and_callback_exception_fail_safely();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
