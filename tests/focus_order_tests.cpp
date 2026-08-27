#include "input/focus_manager.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

struct TestState final {};

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::input::KeyboardInputEvent tab(bool reverse = false) {
    return {
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        reverse ? ryn::input::KeyModifier::shift
                : ryn::input::KeyModifier::none,
        false,
    };
}

void test_focus_order_wraps_and_tracks_dynamic_eligibility() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    std::array<ryn::runtime::ComponentId, 3> component_ids;
    components.mount(ryn::Content{[&] {
        for (auto& component : component_ids) {
            component = mount_leaf();
        }
    }});

    ryn::input::InteractionRegistry registry(components, nodes);
    std::array<ryn::input::InteractionId, 3> interactions;
    for (std::size_t index = 0; index < interactions.size(); ++index) {
        interactions[index] = registry.create({
            component_ids[index],
            components.root(component_ids[index]),
            std::nullopt,
            index != 1,
            true,
            {},
        });
    }
    bool loading = true;
    int loading_activations = 0;
    ryn::input::FocusHandlers loading_handlers;
    loading_handlers.activation_allowed = [&] { return !loading; };
    loading_handlers.activate = [&] { ++loading_activations; };
    registry.set_focus_handlers(interactions[1], std::move(loading_handlers));

    ryn::input::FocusManager focus(registry);
    focus.reserve(interactions.size());
    focus.dispatch(tab());
    require(focus.state().focused == interactions[0],
            "first Tab did not focus the first eligible interaction");
    focus.dispatch(tab());
    require(focus.state().focused == interactions[2],
            "Tab did not skip a disabled interaction");
    focus.dispatch(tab());
    require(focus.state().focused == interactions[0],
            "forward focus traversal did not wrap");
    focus.dispatch(tab(true));
    require(focus.state().focused == interactions[2],
            "reverse focus traversal did not wrap");

    registry.set_eligible(interactions[1], true);
    focus.dispatch(tab());
    focus.dispatch(tab());
    require(focus.state().focused == interactions[1],
            "dynamic eligibility did not update stable declaration order");
    focus.dispatch({
        ryn::input::Key::enter,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    require(loading_activations == 0 && focus.state().focused == interactions[1],
            "loading interaction lost focus or activated");

    registry.set_eligible(interactions[1], false);
    focus.synchronize();
    require(!focus.state().focused.has_value(),
            "disabled focused interaction retained focus");
    focus.dispatch(tab());
    require(focus.state().focused == interactions[0],
            "traversal after dynamic disable did not restart at the first item");

    registry.set_eligible(interactions[0], false);
    registry.set_eligible(interactions[2], false);
    focus.synchronize();
    focus.dispatch(tab());
    require(!focus.state().focused.has_value(),
            "empty focus order produced a focused identity");
}

} // namespace

int main() {
    try {
        test_focus_order_wraps_and_tracks_dynamic_eligibility();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
