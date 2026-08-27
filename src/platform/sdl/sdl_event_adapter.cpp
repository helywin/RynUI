#include "platform/sdl/sdl_event_adapter.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <utility>

namespace ryn::detail {
namespace {

using input::Key;
using input::KeyAction;
using input::KeyModifier;
using input::KeyboardInputEvent;
using input::PlatformInputEvent;
using input::PointerAction;
using input::PointerButton;
using input::PointerIdentity;
using input::PointerInputEvent;
using input::WindowInputAction;
using input::WindowInputEvent;

void append_if_valid(PlatformEvents& result, PlatformInputEvent event) {
    if (input::is_valid(event)) {
        static_cast<void>(result.input.append(std::move(event)));
    }
}

std::optional<Key> map_key(SDL_Keycode key) noexcept {
    switch (key) {
    case SDLK_TAB:
        return Key::tab;
    case SDLK_RETURN:
    case SDLK_RETURN2:
    case SDLK_KP_ENTER:
        return Key::enter;
    case SDLK_SPACE:
        return Key::space;
    default:
        return std::nullopt;
    }
}

KeyModifier map_modifiers(SDL_Keymod modifiers) noexcept {
    KeyModifier result = KeyModifier::none;
    if ((modifiers & SDL_KMOD_SHIFT) != 0) {
        result = result | KeyModifier::shift;
    }
    if ((modifiers & SDL_KMOD_CTRL) != 0) {
        result = result | KeyModifier::control;
    }
    if ((modifiers & SDL_KMOD_ALT) != 0) {
        result = result | KeyModifier::alt;
    }
    if ((modifiers & SDL_KMOD_GUI) != 0) {
        result = result | KeyModifier::meta;
    }
    return result;
}

std::optional<PointerAction> map_touch_action(Uint32 type) noexcept {
    switch (type) {
    case SDL_EVENT_FINGER_DOWN:
        return PointerAction::down;
    case SDL_EVENT_FINGER_UP:
        return PointerAction::up;
    case SDL_EVENT_FINGER_MOTION:
        return PointerAction::move;
    case SDL_EVENT_FINGER_CANCELED:
        return PointerAction::cancel;
    default:
        return std::nullopt;
    }
}

PointerButton button_for(PointerAction action) noexcept {
    return action == PointerAction::down || action == PointerAction::up
        ? PointerButton::primary
        : PointerButton::none;
}

bool is_compatibility_mouse(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        return event.motion.which == SDL_TOUCH_MOUSEID;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
            || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        return event.button.which == SDL_TOUCH_MOUSEID;
    }
    return false;
}

bool is_pen_mouse(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        return event.motion.which == SDL_PEN_MOUSEID;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
            || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        return event.button.which == SDL_PEN_MOUSEID;
    }
    return false;
}

} // namespace

void SdlEventAdapter::merge(
    PlatformEvents& result,
    const SDL_Event& event,
    SdlWindowMetrics& metrics) {
    if (event.type == SDL_EVENT_QUIT
            || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        result.quit_requested = true;
        return;
    }

    result.frame_requested = true;

    if (is_compatibility_mouse(event)) {
        ++result.suppressed_compatibility_mouse_events;
        return;
    }
    if (is_pen_mouse(event)) {
        return;
    }

    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        append_if_valid(result, PointerInputEvent{
            PointerIdentity::mouse(),
            PointerAction::move,
            PointerButton::none,
            event.motion.x,
            event.motion.y,
        });
        return;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT) {
            const auto action = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                ? PointerAction::down
                : PointerAction::up;
            append_if_valid(result, PointerInputEvent{
                PointerIdentity::mouse(),
                action,
                PointerButton::primary,
                event.button.x,
                event.button.y,
            });
        }
        return;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED: {
        const auto action = map_touch_action(event.type);
        if (action.has_value() && metrics.width > 0 && metrics.height > 0) {
            append_if_valid(result, PointerInputEvent{
                PointerIdentity::touch(event.tfinger.touchID, event.tfinger.fingerID),
                *action,
                button_for(*action),
                event.tfinger.x * static_cast<float>(metrics.width),
                event.tfinger.y * static_cast<float>(metrics.height),
            });
        }
        return;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const auto key = map_key(event.key.key);
        if (key.has_value()) {
            append_if_valid(result, KeyboardInputEvent{
                *key,
                event.type == SDL_EVENT_KEY_DOWN ? KeyAction::down : KeyAction::up,
                map_modifiers(event.key.mod),
                event.key.repeat,
            });
        }
        return;
    }
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        append_if_valid(result, WindowInputEvent{WindowInputAction::focus_gained, 0, 0});
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        append_if_valid(result, WindowInputEvent{WindowInputAction::focus_lost, 0, 0});
        return;
    case SDL_EVENT_WINDOW_RESIZED:
        if (event.window.data1 > 0 && event.window.data2 > 0) {
            metrics.width = event.window.data1;
            metrics.height = event.window.data2;
            append_if_valid(result, WindowInputEvent{
                WindowInputAction::resized,
                metrics.width,
                metrics.height,
            });
        }
        return;
    default:
        return;
    }
}

} // namespace ryn::detail
