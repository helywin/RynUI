#include "input/pointer_router.hpp"

#include <ryn/component.hpp>

#include <algorithm>
#include <array>
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

struct ObservedEvent final {
    ryn::input::InteractionId current;
    ryn::input::PointerIdentity pointer;
    ryn::input::PointerEventKind kind;
    std::optional<ryn::input::InteractionId> actual;
    std::optional<ryn::input::InteractionId> press_origin;
};

struct Fixture final {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components{nodes};
    ryn::input::InteractionRegistry registry{components, nodes};
    ryn::input::HitTestSnapshot hit_test{registry, nodes};
    ryn::runtime::FrameRequestState frames;
    ryn::input::PointerRouter router{registry, hit_test, &frames};
    ryn::runtime::ComponentId first_component;
    ryn::runtime::ComponentId second_component;
    ryn::input::InteractionId first;
    ryn::input::InteractionId second;
    std::vector<ObservedEvent> events;
    bool secondary_capture_result{true};

    Fixture() {
        components.mount(ryn::Content{[&] {
            first_component = mount_leaf();
            second_component = mount_leaf();
        }});
        commit(components.root(first_component), {0.0F, 0.0F, 50.0F, 50.0F});
        commit(components.root(second_component), {60.0F, 0.0F, 50.0F, 50.0F});
        first = registry.create({
            first_component, components.root(first_component), std::nullopt, true, true, {}});
        second = registry.create({
            second_component, components.root(second_component), std::nullopt, true, true, {}});
        registry.set_handlers(first, handlers_for(first));
        registry.set_handlers(second, handlers_for(second));
        const std::array entries{
            ryn::input::HitTestPaintEntry{first, std::nullopt},
            ryn::input::HitTestPaintEntry{second, std::nullopt},
        };
        hit_test.rebuild(entries, {0.0F, 0.0F, 120.0F, 80.0F});
        router.reserve(4, 4);
    }

    void commit(ryn::runtime::NodeId id, ryn::runtime::Rect bounds) {
        auto& node = nodes.require(id);
        node.bounds = bounds;
        node.place_generation = 1;
    }

    ryn::input::InteractionHandlers handlers_for(
        ryn::input::InteractionId interaction) {
        ryn::input::InteractionHandlers handlers;
        handlers.target = [this, interaction](ryn::input::PointerDispatchContext& event) {
            events.push_back({
                interaction,
                event.event().pointer,
                event.kind(),
                event.actual_hit_target(),
                event.press_origin(),
            });
            if (event.kind() == ryn::input::PointerEventKind::down) {
                const bool captured = event.capture_pointer();
                if (event.event().button == ryn::input::PointerButton::secondary) {
                    secondary_capture_result = captured;
                }
            }
        };
        return handlers;
    }
};

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerIdentity pointer,
    ryn::input::PointerAction action,
    float x,
    float y,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {pointer, action, button, x, y};
}

void test_hover_follows_actual_hit_while_capture_routes_drag() {
    Fixture fixture;
    const auto mouse = ryn::input::PointerIdentity::mouse();
    fixture.router.dispatch(pointer_event(
        mouse, ryn::input::PointerAction::move, 10.0F, 10.0F));
    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::down,
        10.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    auto state = fixture.router.state(mouse);
    require(state.has_value()
                && state->hover_target == fixture.first
                && state->capture == fixture.first
                && state->press_origin == fixture.first
                && state->primary_down,
            "pointer down did not establish hover/capture/press origin");

    fixture.router.dispatch(pointer_event(
        mouse, ryn::input::PointerAction::move, 70.0F, 10.0F));
    state = fixture.router.state(mouse);
    require(state->hover_target == fixture.second
                && state->capture == fixture.first,
            "drag-out did not separate hover from capture");
    const auto& captured_move = fixture.events.back();
    require(captured_move.current == fixture.first
                && captured_move.kind == ryn::input::PointerEventKind::move
                && captured_move.actual == fixture.second
                && captured_move.press_origin == fixture.first,
            "captured move did not retain actual hit/press origin");

    fixture.router.dispatch(pointer_event(
        mouse, ryn::input::PointerAction::move, 10.0F, 10.0F));
    state = fixture.router.state(mouse);
    require(state->hover_target == fixture.first
                && state->capture == fixture.first,
            "drag-back did not restore actual hover while retaining capture");

    fixture.router.dispatch(pointer_event(
        mouse, ryn::input::PointerAction::move, 70.0F, 10.0F));
    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::up,
        70.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    state = fixture.router.state(mouse);
    require(state->hover_target == fixture.second
                && !state->capture.has_value()
                && !state->press_origin.has_value()
                && !state->primary_down,
            "captured up did not release primary pointer state");
    const auto up = std::find_if(
        fixture.events.rbegin(),
        fixture.events.rend(),
        [](const auto& event) {
            return event.kind == ryn::input::PointerEventKind::up;
        });
    require(up != fixture.events.rend()
                && up->current == fixture.first
                && up->actual == fixture.second
                && up->press_origin == fixture.first,
            "captured up was not delivered to the press target");
}

void test_pointer_identities_and_secondary_button_are_isolated() {
    Fixture fixture;
    const auto first_touch = ryn::input::PointerIdentity::touch(1, 11);
    const auto second_touch = ryn::input::PointerIdentity::touch(1, 12);
    fixture.router.dispatch(pointer_event(
        first_touch,
        ryn::input::PointerAction::down,
        10.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    fixture.router.dispatch(pointer_event(
        second_touch,
        ryn::input::PointerAction::down,
        70.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    require(fixture.router.pointer_count() == 2,
            "touch identities shared pointer state");
    require(fixture.router.state(first_touch)->capture == fixture.first
                && fixture.router.state(second_touch)->capture == fixture.second,
            "touch captures were not isolated");

    fixture.router.dispatch(pointer_event(
        first_touch,
        ryn::input::PointerAction::up,
        70.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    require(!fixture.router.state(first_touch)->capture.has_value()
                && fixture.router.state(second_touch)->capture == fixture.second,
            "one touch up released another touch capture");
    require(!fixture.router.state(first_touch)->hover_target.has_value(),
            "touch up retained hover state");

    const auto mouse = ryn::input::PointerIdentity::mouse();
    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::down,
        10.0F,
        10.0F,
        ryn::input::PointerButton::secondary));
    const auto mouse_state = fixture.router.state(mouse);
    require(!fixture.secondary_capture_result
                && !mouse_state->primary_down
                && !mouse_state->capture.has_value()
                && !mouse_state->press_origin.has_value(),
            "secondary button entered primary capture/press state");
}

void test_cancel_and_window_focus_loss_clear_capture_and_hover() {
    Fixture fixture;
    const auto mouse = ryn::input::PointerIdentity::mouse();
    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::down,
        10.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    fixture.router.cancel_all();
    auto state = fixture.router.state(mouse);
    require(!state->primary_down
                && !state->capture.has_value()
                && !state->press_origin.has_value()
                && !state->hover_target.has_value(),
            "window focus loss retained pointer state");
    require(std::any_of(
                fixture.events.begin(),
                fixture.events.end(),
                [&](const auto& event) {
                    return event.current == fixture.first
                        && event.kind == ryn::input::PointerEventKind::cancel;
                }),
            "window focus loss did not route cancel to capture target");

    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::down,
        10.0F,
        10.0F,
        ryn::input::PointerButton::primary));
    fixture.router.dispatch(pointer_event(
        mouse,
        ryn::input::PointerAction::cancel,
        10.0F,
        10.0F));
    state = fixture.router.state(mouse);
    require(!state->capture.has_value()
                && !state->press_origin.has_value()
                && !state->hover_target.has_value(),
            "explicit pointer cancel retained state");
    require(fixture.router.diagnostics().cancels == 2,
            "cancel diagnostics differ");
}

} // namespace

int main() {
    try {
        test_hover_follows_actual_hit_while_capture_routes_drag();
        test_pointer_identities_and_secondary_button_are_isolated();
        test_cancel_and_window_focus_loss_clear_capture_and_hover();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
