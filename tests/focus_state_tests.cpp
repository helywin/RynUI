#include "input/focus_manager.hpp"
#include "input/pointer_router.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

ryn::runtime::ComponentId mount_parent(const Children& children) {
    auto& build = ryn::runtime::require_component_build_context();
    const auto parent = build.mount_component<TestState>();
    build.mount_slot(parent, children);
    return parent;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_pointer_keyboard_modality_and_window_restoration() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId child_component;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            child_component = mount_leaf();
        }});
    }});
    auto commit = [&](ryn::runtime::NodeId node, ryn::runtime::Rect bounds) {
        auto& record = nodes.require(node);
        record.bounds = bounds;
        record.place_generation = 1;
    };
    commit(components.root(parent_component), {0.0F, 0.0F, 100.0F, 100.0F});
    commit(components.root(child_component), {10.0F, 10.0F, 80.0F, 80.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto parent = registry.create({
        parent_component,
        components.root(parent_component),
        std::nullopt,
        true,
        true,
        {},
    });
    const auto child = registry.create({
        child_component,
        components.root(child_component),
        parent,
        true,
        false,
        {},
    });
    std::vector<ryn::input::FocusPresentation> presentations;
    ryn::input::FocusHandlers handlers;
    handlers.state_changed = [&](ryn::input::FocusPresentation state) {
        presentations.push_back(state);
    };
    registry.set_focus_handlers(parent, std::move(handlers));

    ryn::input::HitTestSnapshot hit_test(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{parent, std::nullopt},
        ryn::input::HitTestPaintEntry{child, std::nullopt},
    };
    hit_test.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});
    ryn::runtime::FrameRequestState frames;
    ryn::input::FocusManager focus(registry, &frames);
    ryn::input::PointerRouter pointer(registry, hit_test, &frames, &focus);
    focus.reserve(2);
    pointer.reserve(1, 4);

    pointer.dispatch({
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::down,
        ryn::input::PointerButton::primary,
        20.0F,
        20.0F,
    });
    auto state = focus.state();
    require(state.focused == parent
                && state.modality == ryn::input::FocusModality::pointer
                && !state.focus_visible,
            "nested pointer target did not focus its focusable ancestor");
    require(!presentations.empty()
                && presentations.back() == ryn::input::FocusPresentation{true, false, false},
            "pointer focus presentation differs");

    focus.dispatch({
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    state = focus.state();
    require(state.focused == parent
                && state.modality == ryn::input::FocusModality::keyboard
                && state.focus_visible,
            "keyboard traversal did not expose focus-visible");

    focus.set_window_active(false);
    state = focus.state();
    require(state.focused == parent && !state.window_active
                && !state.focus_visible,
            "window blur did not preserve identity while hiding focus-visible");
    focus.set_window_active(true);
    state = focus.state();
    require(state.focused == parent && state.window_active
                && state.focus_visible,
            "window focus gain did not restore valid keyboard focus-visible");

    registry.set_eligible(parent, false);
    focus.synchronize();
    require(!focus.state().focused.has_value(),
            "disabled focused target was not cleared");
    require(frames.pending(), "focus transitions did not request a frame");
}

} // namespace

int main() {
    try {
        test_pointer_keyboard_modality_and_window_restoration();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
