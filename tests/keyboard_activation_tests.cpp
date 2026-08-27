#include "input/focus_manager.hpp"

#include <ryn/component.hpp>

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

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

ryn::input::KeyboardInputEvent key(
    ryn::input::Key value,
    ryn::input::KeyAction action,
    bool repeat = false) {
    return {value, action, ryn::input::KeyModifier::none, repeat};
}

void test_enter_space_and_dynamic_activation_gate() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    ryn::input::InteractionRegistry registry(components, nodes);
    const auto interaction = registry.create({
        component,
        components.root(component),
        std::nullopt,
        true,
        true,
        {},
    });

    bool enabled = true;
    bool loading = false;
    int activations = 0;
    std::vector<ryn::input::FocusPresentation> presentations;
    ryn::input::FocusHandlers handlers;
    handlers.state_changed = [&](ryn::input::FocusPresentation state) {
        presentations.push_back(state);
    };
    handlers.activation_allowed = [&] { return enabled && !loading; };
    handlers.activate = [&] { ++activations; };
    registry.set_focus_handlers(interaction, std::move(handlers));

    ryn::input::FocusManager focus(registry);
    require(focus.request_focus(interaction, ryn::input::FocusModality::keyboard),
            "keyboard activation setup did not focus target");

    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down, true));
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::up));
    require(activations == 1,
            "Enter repeat or key up produced more than one activation");

    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
    require(focus.state().keyboard_pressed
                && presentations.back().keyboard_pressed,
            "Space key down did not enter keyboard pressed state");
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down, true));
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::up));
    require(focus.state().keyboard_pressed && activations == 1,
            "repeat or mismatched key cleared/activated Space press");
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::up));
    require(!focus.state().keyboard_pressed && activations == 2,
            "matching Space key up did not close pressed before activation");

    loading = true;
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
    require(!focus.state().keyboard_pressed && activations == 2,
            "loading target entered pressed state");
    loading = false;
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
    loading = true;
    focus.synchronize();
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::up));
    require(!focus.state().keyboard_pressed && activations == 2,
            "loading transition during Space press produced activation");

    loading = false;
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
    focus.set_window_active(false);
    focus.set_window_active(true);
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::up));
    require(!focus.state().keyboard_pressed && activations == 2,
            "window blur did not cancel pending Space activation");

    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
    registry.set_eligible(interaction, false);
    focus.synchronize();
    focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::up));
    require(!focus.state().focused.has_value()
                && !focus.state().keyboard_pressed
                && activations == 2,
            "disable during Space press retained focus or produced activation");
    registry.set_eligible(interaction, true);
    focus.request_focus(interaction, ryn::input::FocusModality::keyboard);
    enabled = false;
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    require(activations == 2, "disabled activation gate produced a click");
}

void test_activation_callback_mutation_does_not_retarget_reused_slot() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    ryn::input::InteractionRegistry registry(components, nodes);
    const auto original = registry.create({
        component,
        components.root(component),
        std::nullopt,
        true,
        true,
        {},
    });
    ryn::input::FocusManager focus(registry);
    int original_activations = 0;
    int replacement_activations = 0;
    ryn::input::InteractionId replacement;
    ryn::input::FocusHandlers original_handlers;
    original_handlers.activate = [&] {
        ++original_activations;
        require(registry.remove(original), "activation callback remove failed");
        replacement = registry.create({
            component,
            components.root(component),
            std::nullopt,
            true,
            true,
            {},
        });
        ryn::input::FocusHandlers replacement_handlers;
        replacement_handlers.activate = [&] { ++replacement_activations; };
        registry.set_focus_handlers(replacement, std::move(replacement_handlers));
    };
    registry.set_focus_handlers(original, std::move(original_handlers));
    focus.request_focus(original, ryn::input::FocusModality::keyboard);

    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    require(original_activations == 1
                && replacement.index == original.index
                && replacement.generation != original.generation,
            "activation callback did not reuse the interaction generation");
    require(!focus.state().focused.has_value(),
            "reused interaction slot inherited stale focus");
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    require(replacement_activations == 0,
            "replacement interaction received the old keyboard sequence");

    require(focus.focus_from_pointer(replacement),
            "pointer focus did not select replacement interaction");
    require(replacement_activations == 0,
            "pointer focus was incorrectly treated as activation");
    focus.dispatch(key(ryn::input::Key::enter, ryn::input::KeyAction::down));
    require(replacement_activations == 1,
            "fresh keyboard activation did not reach replacement interaction");
}

} // namespace

int main() {
    try {
        test_enter_space_and_dynamic_activation_gate();
        test_activation_callback_mutation_does_not_retarget_reused_slot();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
