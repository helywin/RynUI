#include "platform/sdl/sdl_event_adapter.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using ryn::detail::PlatformEvents;
using ryn::detail::SdlEventAdapter;
using ryn::detail::SdlWindowMetrics;
using ryn::input::Key;
using ryn::input::KeyAction;
using ryn::input::KeyModifier;
using ryn::input::KeyboardInputEvent;
using ryn::input::PointerAction;
using ryn::input::PointerButton;
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

bool near(float left, float right) noexcept {
    return std::abs(left - right) < 0.001F;
}

SDL_Event touch_event(
    SDL_EventType type,
    SDL_TouchID touch,
    SDL_FingerID finger,
    float x,
    float y) {
    SDL_Event event{};
    event.type = type;
    event.tfinger.touchID = touch;
    event.tfinger.fingerID = finger;
    event.tfinger.x = x;
    event.tfinger.y = y;
    return event;
}

SDL_Event mouse_motion(SDL_MouseID source, float x, float y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.which = source;
    event.motion.x = x;
    event.motion.y = y;
    return event;
}

SDL_Event mouse_button(SDL_EventType type, SDL_MouseID source, Uint8 button) {
    SDL_Event event{};
    event.type = type;
    event.button.which = source;
    event.button.button = button;
    event.button.x = 32.0F;
    event.button.y = 48.0F;
    return event;
}

void test_touch_sequence_uses_logical_coordinates_and_identity() {
    PlatformEvents result;
    result.input.reserve(8);
    SdlWindowMetrics metrics{800, 600};

    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_DOWN, 7, 11, 0.25F, 0.5F), metrics);
    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_MOTION, 7, 11, 0.5F, 0.25F), metrics);
    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_UP, 7, 11, 0.5F, 0.25F), metrics);
    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_CANCELED, 7, 12, 0.1F, 0.2F), metrics);

    require(result.frame_requested, "touch sequence did not request a frame");
    require(result.input.size() == 4, "touch sequence lost an action");
    const auto& down = std::get<PointerInputEvent>(result.input.events()[0]);
    const auto& move = std::get<PointerInputEvent>(result.input.events()[1]);
    const auto& up = std::get<PointerInputEvent>(result.input.events()[2]);
    const auto& cancel = std::get<PointerInputEvent>(result.input.events()[3]);
    require(down.pointer == PointerIdentity::touch(7, 11),
            "touch device/finger identity was not preserved");
    require(down.action == PointerAction::down && down.button == PointerButton::primary,
            "touch down mapping differs");
    require(down.x == 200.0F && down.y == 300.0F,
            "touch down was not converted to logical coordinates");
    require(move.action == PointerAction::move
                && move.button == PointerButton::none
                && move.x == 400.0F
                && move.y == 150.0F,
            "touch motion mapping differs");
    require(up.action == PointerAction::up && up.button == PointerButton::primary,
            "touch up mapping differs");
    require(cancel.pointer == PointerIdentity::touch(7, 12)
                && cancel.action == PointerAction::cancel
                && cancel.button == PointerButton::none,
            "touch cancel mapping differs");
}

void test_compatibility_mouse_is_suppressed_without_hiding_real_mouse() {
    PlatformEvents result;
    result.input.reserve(8);
    SdlWindowMetrics metrics{640, 480};

    SdlEventAdapter::merge(result, mouse_motion(SDL_TOUCH_MOUSEID, 10.0F, 20.0F), metrics);
    SdlEventAdapter::merge(
        result,
        mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT),
        metrics);
    require(result.input.empty(), "touch compatibility mouse event was duplicated");
    require(result.suppressed_compatibility_mouse_events == 2,
            "compatibility mouse suppression count differs");

    SdlEventAdapter::merge(result, mouse_motion(3, 15.0F, 25.0F), metrics);
    SdlEventAdapter::merge(
        result,
        mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, 3, SDL_BUTTON_LEFT),
        metrics);
    SdlEventAdapter::merge(
        result,
        mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, 3, SDL_BUTTON_LEFT),
        metrics);
    require(result.input.size() == 3, "real mouse sequence was not normalized");
    const auto& move = std::get<PointerInputEvent>(result.input.events()[0]);
    const auto& down = std::get<PointerInputEvent>(result.input.events()[1]);
    const auto& up = std::get<PointerInputEvent>(result.input.events()[2]);
    require(move.pointer == PointerIdentity::mouse()
                && move.action == PointerAction::move
                && move.x == 15.0F
                && move.y == 25.0F,
            "real mouse move mapping differs");
    require(down.action == PointerAction::down && up.action == PointerAction::up,
            "real mouse button mapping differs");
}

void test_keyboard_focus_resize_and_cancel_order() {
    PlatformEvents result;
    result.input.reserve(8);
    SdlWindowMetrics metrics{320, 240};

    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.key = SDLK_TAB;
    key.key.mod = static_cast<SDL_Keymod>(SDL_KMOD_SHIFT | SDL_KMOD_CTRL);
    key.key.repeat = true;
    SdlEventAdapter::merge(result, key, metrics);

    SDL_Event focus_lost{};
    focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    SdlEventAdapter::merge(result, focus_lost, metrics);
    SDL_Event focus_gained{};
    focus_gained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    SdlEventAdapter::merge(result, focus_gained, metrics);
    SDL_Event resized{};
    resized.type = SDL_EVENT_WINDOW_RESIZED;
    resized.window.data1 = 1024;
    resized.window.data2 = 768;
    SdlEventAdapter::merge(result, resized, metrics);
    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_DOWN, 2, 3, 0.5F, 0.5F), metrics);

    require(result.input.size() == 5, "keyboard/window event order changed");
    const auto& mapped_key = std::get<KeyboardInputEvent>(result.input.events()[0]);
    require(mapped_key.key == Key::tab
                && mapped_key.action == KeyAction::down
                && mapped_key.repeat,
            "key down/repeat mapping differs");
    require(ryn::input::has_modifier(mapped_key.modifiers, KeyModifier::shift)
                && ryn::input::has_modifier(mapped_key.modifiers, KeyModifier::control),
            "SDL key modifiers were not normalized");
    require(std::get<WindowInputEvent>(result.input.events()[1]).action
                == WindowInputAction::focus_lost,
            "focus lost order differs");
    require(std::get<WindowInputEvent>(result.input.events()[2]).action
                == WindowInputAction::focus_gained,
            "focus gained order differs");
    const auto& resize = std::get<WindowInputEvent>(result.input.events()[3]);
    require(resize.action == WindowInputAction::resized
                && resize.width == 1024
                && resize.height == 768,
            "window resize mapping differs");
    const auto& touch = std::get<PointerInputEvent>(result.input.events()[4]);
    require(touch.x == 512.0F && touch.y == 384.0F,
            "resize did not update touch logical conversion");
}

void test_display_scale_maps_pixels_and_pointer_to_logical_coordinates() {
    PlatformEvents result;
    result.input.reserve(8);
    SdlWindowMetrics metrics{960, 720, 960, 720, 1.0F, 1.5F};

    SdlEventAdapter::merge(result, mouse_motion(7, 150.0F, 90.0F), metrics);
    SdlEventAdapter::merge(
        result, touch_event(SDL_EVENT_FINGER_DOWN, 4, 5, 0.5F, 0.25F), metrics);

    SDL_Event pixel_size{};
    pixel_size.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    pixel_size.window.data1 = 1200;
    pixel_size.window.data2 = 900;
    SdlEventAdapter::merge(result, pixel_size, metrics);

    require(result.input.size() == 3, "DPI mapping lost normalized input");
    const auto& mouse = std::get<PointerInputEvent>(result.input.events()[0]);
    const auto& touch = std::get<PointerInputEvent>(result.input.events()[1]);
    const auto& resize = std::get<WindowInputEvent>(result.input.events()[2]);
    require(near(mouse.x, 100.0F) && near(mouse.y, 60.0F),
            "mouse coordinates were not converted from Window coordinates to logical UI");
    require(near(touch.x, 320.0F) && near(touch.y, 120.0F),
            "touch coordinates were not converted to logical UI");
    require(resize.action == WindowInputAction::resized
                && resize.width == 800
                && resize.height == 600,
            "drawable pixel resize was not divided by display scale");

    PlatformEvents scale_result;
    metrics.display_scale = 2.0F;
    SDL_Event scale_changed{};
    scale_changed.type = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED;
    SdlEventAdapter::merge(scale_result, scale_changed, metrics);
    require(scale_result.input.size() == 1,
            "display-scale change did not emit a logical viewport update");
    const auto& scaled = std::get<WindowInputEvent>(
        scale_result.input.events().front());
    require(scaled.width == 600 && scaled.height == 450,
            "display-scale change retained the stale logical viewport");
}

void test_wheel_precision_direction_and_logical_position() {
    PlatformEvents result;
    result.input.reserve(8);
    SdlWindowMetrics metrics{960, 720, 1920, 1440, 2.0F, 1.5F};

    SDL_Event normal{};
    normal.type = SDL_EVENT_MOUSE_WHEEL;
    normal.wheel.x = 0.25F;
    normal.wheel.y = -1.5F;
    normal.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    normal.wheel.mouse_x = 150.0F;
    normal.wheel.mouse_y = 75.0F;
    SdlEventAdapter::merge(result, normal, metrics);

    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    SdlEventAdapter::merge(result, focus, metrics);

    SDL_Event flipped{};
    flipped.type = SDL_EVENT_MOUSE_WHEEL;
    flipped.wheel.x = 0.5F;
    flipped.wheel.y = -2.0F;
    flipped.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    flipped.wheel.mouse_x = 30.0F;
    flipped.wheel.mouse_y = 60.0F;
    SdlEventAdapter::merge(result, flipped, metrics);

    require(result.input.size() == 3,
            "wheel/focus batch lost normalized event order");
    const auto& first = std::get<ScrollInputEvent>(result.input.events()[0]);
    const auto& second = std::get<ScrollInputEvent>(result.input.events()[2]);
    require(near(first.delta_x, 0.25F) && near(first.delta_y, -1.5F)
                && near(first.x, 200.0F) && near(first.y, 100.0F),
            "normal precise wheel values or logical pointer position changed");
    require(near(second.delta_x, -0.5F) && near(second.delta_y, 2.0F)
                && near(second.x, 40.0F) && near(second.y, 80.0F),
            "flipped wheel direction was not normalized exactly once");

    PlatformEvents rejected;
    auto invalid = normal;
    invalid.wheel.direction = static_cast<SDL_MouseWheelDirection>(99);
    SdlEventAdapter::merge(rejected, invalid, metrics);
    auto empty = normal;
    empty.wheel.x = 0.0F;
    empty.wheel.y = 0.0F;
    SdlEventAdapter::merge(rejected, empty, metrics);
    require(rejected.input.empty(),
            "invalid or empty wheel input entered the normalized batch");
}

void test_quit_and_frame_summary_regression() {
    SdlWindowMetrics metrics{640, 480};

    PlatformEvents quit_result;
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    SdlEventAdapter::merge(quit_result, quit, metrics);
    require(quit_result.quit_requested, "SDL quit was not preserved");
    require(!quit_result.frame_requested, "quit unexpectedly changed frame summary semantics");

    PlatformEvents close_result;
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    SdlEventAdapter::merge(close_result, close, metrics);
    require(close_result.quit_requested, "window close was not preserved");

    PlatformEvents resize_result;
    SDL_Event unhandled{};
    unhandled.type = SDL_EVENT_USER;
    SdlEventAdapter::merge(resize_result, unhandled, metrics);
    require(resize_result.frame_requested, "unhandled SDL event no longer requests a frame");
    require(resize_result.input.empty(), "unhandled SDL event leaked into normalized input");

    PlatformEvents secondary_button_result;
    SdlEventAdapter::merge(
        secondary_button_result,
        mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, 0, SDL_BUTTON_RIGHT),
        metrics);
    require(secondary_button_result.frame_requested,
            "secondary mouse button no longer requests a frame");
    require(secondary_button_result.input.empty(),
            "unsupported mouse button entered normalized input");
}

void test_expose_redraw_survives_a_mixed_input_batch() {
    PlatformEvents result;
    result.input.reserve(4);
    SdlWindowMetrics metrics{640, 480};

    SdlEventAdapter::merge(result, mouse_motion(1, 12.0F, 24.0F), metrics);
    SDL_Event exposed{};
    exposed.type = SDL_EVENT_WINDOW_EXPOSED;
    SdlEventAdapter::merge(result, exposed, metrics);

    require(result.input.size() == 1,
            "mixed expose batch lost its pointer input");
    require(result.frame_requested,
            "mixed expose batch lost its generic frame request");
    require(result.redraw_requested,
            "mixed expose batch lost its mandatory redraw request");

    result.clear();
    require(!result.redraw_requested,
            "event-batch clear retained a stale redraw request");
}

} // namespace

int main() {
    try {
        test_touch_sequence_uses_logical_coordinates_and_identity();
        test_compatibility_mouse_is_suppressed_without_hiding_real_mouse();
        test_keyboard_focus_resize_and_cancel_order();
        test_display_scale_maps_pixels_and_pointer_to_logical_coordinates();
        test_wheel_precision_direction_and_logical_position();
        test_quit_and_frame_summary_regression();
        test_expose_redraw_survives_a_mixed_input_batch();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
