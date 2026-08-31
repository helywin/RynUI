#include "input/platform_input.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using ryn::input::Key;
using ryn::input::KeyAction;
using ryn::input::KeyModifier;
using ryn::input::KeyboardInputEvent;
using ryn::input::PlatformInputBatch;
using ryn::input::PointerAction;
using ryn::input::PointerButton;
using ryn::input::PointerDevice;
using ryn::input::PointerIdentity;
using ryn::input::PointerInputEvent;
using ryn::input::ScrollInputEvent;
using ryn::input::WindowInputAction;
using ryn::input::WindowInputEvent;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_pointer_identity_and_logical_values() {
    constexpr auto mouse = PointerIdentity::mouse();
    constexpr auto first_touch = PointerIdentity::touch(4, 21);
    constexpr auto second_touch = PointerIdentity::touch(4, 22);

    require(mouse.device == PointerDevice::mouse, "mouse identity lost its device tag");
    require(mouse == PointerIdentity::mouse(), "mouse identity is not stable");
    require(first_touch != second_touch, "touch finger identities alias");
    require(first_touch != mouse, "touch identity aliases reserved mouse identity");

    const PointerInputEvent mouse_down{
        mouse,
        PointerAction::down,
        PointerButton::primary,
        123.5F,
        45.25F,
    };
    const PointerInputEvent touch_move{
        first_touch,
        PointerAction::move,
        PointerButton::none,
        400.0F,
        300.0F,
    };
    require(ryn::input::is_valid(mouse_down), "valid mouse down was rejected");
    require(ryn::input::is_valid(touch_move), "valid touch move was rejected");
    require(ryn::input::is_valid(PointerInputEvent{
                mouse,
                PointerAction::down,
                PointerButton::secondary,
                0.0F,
                0.0F,
            }),
            "valid secondary pointer value was rejected");
    require(mouse_down.x == 123.5F && mouse_down.y == 45.25F,
            "logical pointer coordinates changed");
}

void test_keyboard_repeat_and_modifiers() {
    const KeyboardInputEvent repeated_tab{
        Key::tab,
        KeyAction::down,
        KeyModifier::shift | KeyModifier::control,
        true,
    };
    require(ryn::input::is_valid(repeated_tab), "valid repeated key was rejected");
    require(repeated_tab.repeat, "key repeat flag was lost");
    require(ryn::input::has_modifier(repeated_tab.modifiers, KeyModifier::shift),
            "shift modifier was lost");
    require(ryn::input::has_modifier(repeated_tab.modifiers, KeyModifier::control),
            "control modifier was lost");
    require(!ryn::input::has_modifier(repeated_tab.modifiers, KeyModifier::alt),
            "unexpected alt modifier was added");
}

void test_scroll_values_are_precise_and_platform_neutral() {
    const ScrollInputEvent scroll{0.25F, -1.5F, 123.5F, 45.25F};
    require(ryn::input::is_valid(scroll), "valid precise scroll was rejected");
    require(scroll.delta_x == 0.25F && scroll.delta_y == -1.5F
                && scroll.x == 123.5F && scroll.y == 45.25F,
            "scroll precision or logical pointer position changed");
    require(!ryn::input::is_valid(ScrollInputEvent{}),
            "empty scroll input was accepted");
}

void test_batch_reuses_capacity_and_respects_move_boundaries() {
    PlatformInputBatch batch;
    batch.reserve(8);
    const auto reserved_capacity = batch.capacity();
    const auto mouse = PointerIdentity::mouse();

    require(batch.append(PointerInputEvent{
                mouse, PointerAction::move, PointerButton::none, 1.0F, 2.0F}),
            "first mouse move was not appended");
    require(!batch.append(PointerInputEvent{
                mouse, PointerAction::move, PointerButton::none, 3.0F, 4.0F}),
            "consecutive mouse move was not coalesced");
    require(batch.size() == 1, "coalesced move grew the batch");
    require(batch.coalesced_move_count() == 1, "move coalescing was not diagnosed");
    require(std::get<PointerInputEvent>(batch.events().front()).x == 3.0F,
            "coalescing did not keep the latest logical position");

    require(batch.append(KeyboardInputEvent{
                Key::space, KeyAction::down, KeyModifier::none, false}),
            "key boundary was not appended");
    require(batch.append(ScrollInputEvent{0.0F, -0.5F, 3.0F, 4.0F}),
            "scroll boundary was not appended");
    require(batch.append(PointerInputEvent{
                mouse, PointerAction::move, PointerButton::none, 5.0F, 6.0F}),
            "move crossed a key boundary");
    require(batch.append(PointerInputEvent{
                mouse, PointerAction::down, PointerButton::primary, 5.0F, 6.0F}),
            "pointer down was not appended");
    require(batch.append(PointerInputEvent{
                mouse, PointerAction::move, PointerButton::none, 7.0F, 8.0F}),
            "move crossed a pointer button boundary");
    require(batch.append(PointerInputEvent{
                PointerIdentity::touch(9, 10),
                PointerAction::move,
                PointerButton::none,
                9.0F,
                10.0F}),
            "different pointer move was not appended");
    require(batch.size() == 7, "event boundaries did not preserve order");
    require(std::holds_alternative<KeyboardInputEvent>(batch.events()[1])
                && std::holds_alternative<ScrollInputEvent>(batch.events()[2])
                && std::holds_alternative<PointerInputEvent>(batch.events()[3]),
            "scroll input changed surrounding event order");

    batch.clear();
    require(batch.empty(), "batch clear retained events");
    require(batch.capacity() == reserved_capacity, "batch clear released reusable capacity");
    require(batch.coalesced_move_count() == 0, "batch clear retained diagnostics");

    for (int index = 0; index < 8; ++index) {
        const auto action = index % 2 == 0 ? PointerAction::down : PointerAction::up;
        static_cast<void>(batch.append(PointerInputEvent{
            mouse,
            action,
            PointerButton::primary,
            static_cast<float>(index),
            0.0F,
        }));
    }
    require(batch.capacity() == reserved_capacity,
            "batch allocated before reaching reserved capacity");
}

void test_invalid_values_are_rejected_without_mutation() {
    PlatformInputBatch batch;
    batch.reserve(4);
    static_cast<void>(batch.append(WindowInputEvent{
        WindowInputAction::focus_gained, 0, 0}));
    const auto size_before = batch.size();
    const auto capacity_before = batch.capacity();

    bool rejected_nan = false;
    try {
        static_cast<void>(batch.append(PointerInputEvent{
            PointerIdentity::mouse(),
            PointerAction::move,
            PointerButton::none,
            std::numeric_limits<float>::quiet_NaN(),
            0.0F,
        }));
    } catch (const std::invalid_argument&) {
        rejected_nan = true;
    }
    require(rejected_nan, "NaN pointer coordinate was accepted");

    bool rejected_identity = false;
    try {
        static_cast<void>(batch.append(PointerInputEvent{
            {PointerDevice::mouse, 1, 0},
            PointerAction::down,
            PointerButton::primary,
            0.0F,
            0.0F,
        }));
    } catch (const std::invalid_argument&) {
        rejected_identity = true;
    }
    require(rejected_identity, "non-reserved mouse identity was accepted");

    bool rejected_key = false;
    try {
        static_cast<void>(batch.append(KeyboardInputEvent{
            Key::invalid, KeyAction::down, KeyModifier::none, false}));
    } catch (const std::invalid_argument&) {
        rejected_key = true;
    }
    require(rejected_key, "invalid key was accepted");

    bool rejected_unknown_key = false;
    try {
        static_cast<void>(batch.append(KeyboardInputEvent{
            static_cast<Key>(99), KeyAction::down, KeyModifier::none, false}));
    } catch (const std::invalid_argument&) {
        rejected_unknown_key = true;
    }
    require(rejected_unknown_key, "unknown key tag was accepted");

    bool rejected_scroll = false;
    try {
        static_cast<void>(batch.append(ScrollInputEvent{
            0.0F,
            std::numeric_limits<float>::quiet_NaN(),
            0.0F,
            0.0F,
        }));
    } catch (const std::invalid_argument&) {
        rejected_scroll = true;
    }
    require(rejected_scroll, "invalid scroll delta was accepted");

    bool rejected_resize = false;
    try {
        static_cast<void>(batch.append(WindowInputEvent{
            WindowInputAction::resized, 0, 480}));
    } catch (const std::invalid_argument&) {
        rejected_resize = true;
    }
    require(rejected_resize, "invalid window size was accepted");
    require(batch.size() == size_before, "invalid input mutated the batch");
    require(batch.capacity() == capacity_before, "invalid input changed batch capacity");
}

} // namespace

int main() {
    try {
        test_pointer_identity_and_logical_values();
        test_keyboard_repeat_and_modifiers();
        test_scroll_values_are_precise_and_platform_neutral();
        test_batch_reuses_capacity_and_respects_move_boundaries();
        test_invalid_values_are_rejected_without_mutation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
