#include "platform/sdl/sdl_event_adapter.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
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
using input::ScrollInputEvent;
using input::WindowInputAction;
using input::WindowInputEvent;

void append_if_valid(PlatformEvents& result, PlatformInputEvent event) {
    if (input::is_valid(event)) {
        static_cast<void>(result.input.append(std::move(event)));
    }
}

std::string_view bounded_text(const char* text) {
    if(!text) throw std::invalid_argument("Null text event payload");
    std::size_t bytes = 0;
    while(bytes <= input::text_event_max_bytes && text[bytes] != '\0') ++bytes;
    if(bytes > input::text_event_max_bytes) throw std::length_error("Text event exceeds payload limit");
    return {text, bytes};
}

String owned_text(const char* text) {
    auto parsed = String::from_utf8(bounded_text(text));
    if(!parsed) throw std::invalid_argument("Text event contains invalid UTF-8");
    return std::move(parsed).value();
}

bool text_window_matches(const PlatformEvents& result, Uint32 window, Uint64 timestamp) noexcept {
    return (result.window_id == 0 || result.window_id == window)
        && (timestamp == 0 || timestamp >= result.text_started_at);
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

float to_logical_coordinate(float value, const SdlWindowMetrics& metrics) noexcept {
    return value * metrics.coordinate_to_logical_scale();
}

int rounded_logical_extent(float value) noexcept {
    return std::max(1, static_cast<int>(std::lround(value)));
}

std::optional<float> wheel_direction_sign(
    SDL_MouseWheelDirection direction) noexcept {
    switch (direction) {
    case SDL_MOUSEWHEEL_NORMAL:
        return 1.0F;
    case SDL_MOUSEWHEEL_FLIPPED:
        return -1.0F;
    default:
        return std::nullopt;
    }
}

void append_logical_resize(
    PlatformEvents& result,
    const SdlWindowMetrics& metrics) {
    const float width = metrics.logical_width();
    const float height = metrics.logical_height();
    if (width > 0.0F && height > 0.0F) {
        append_if_valid(result, WindowInputEvent{
            WindowInputAction::resized,
            rounded_logical_extent(width),
            rounded_logical_extent(height),
        });
    }
}

} // namespace

static void merge_event(
    PlatformEvents& result,
    const SDL_Event& event,
    SdlWindowMetrics& metrics) {
    if (event.type == SDL_EVENT_QUIT
            || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        result.quit_requested = true;
        return;
    }

    result.frame_requested = true;
    if (event.type == SDL_EVENT_WINDOW_EXPOSED) {
        result.redraw_requested = true;
    }

    if (is_compatibility_mouse(event)) {
        ++result.suppressed_compatibility_mouse_events;
        return;
    }
    if (is_pen_mouse(event)) {
        return;
    }

    switch (event.type) {
    case SDL_EVENT_CLIPBOARD_UPDATE:
        if(event.clipboard.num_mime_types < 0 || event.clipboard.num_mime_types > 256)
            throw std::invalid_argument("Invalid clipboard format count");
        result.input.append(input::ClipboardChanged{event.clipboard.owner,
            static_cast<std::uint32_t>(event.clipboard.num_mime_types)});
        return;
    case SDL_EVENT_TEXT_INPUT: {
        if(!text_window_matches(result, event.text.windowID, event.text.timestamp)) return;
        result.input.append(input::TextCommitted{owned_text(event.text.text), result.text_session});
        return;
    }
    case SDL_EVENT_TEXT_EDITING: {
        if(!text_window_matches(result, event.edit.windowID, event.edit.timestamp)) return;
        if(event.edit.start < -1 || event.edit.length < -1)
            throw std::invalid_argument("Invalid composition character range");
        const auto range = event.edit.start == -1 || event.edit.length == -1
            ? input::TextScalarRange{0, 0, false}
            : input::TextScalarRange{static_cast<std::size_t>(event.edit.start),
                static_cast<std::size_t>(event.edit.length), true};
        result.input.append(input::CompositionChanged{owned_text(event.edit.text), range, result.text_session});
        return;
    }
    case SDL_EVENT_TEXT_EDITING_CANDIDATES: {
        if(!text_window_matches(result, event.edit_candidates.windowID, event.edit_candidates.timestamp)) return;
        const auto& source = event.edit_candidates;
        if(source.num_candidates < 0 || source.num_candidates > static_cast<Sint32>(input::text_event_max_candidates)
            || (source.num_candidates != 0 && !source.candidates)
            || source.selected_candidate < -1 || source.selected_candidate >= source.num_candidates)
            throw std::invalid_argument("Invalid candidate snapshot");
        std::size_t bytes = 0;
        for(Sint32 index = 0; index < source.num_candidates; ++index) {
            const auto size = bounded_text(source.candidates[index]).size();
            if(size > input::text_event_max_bytes - bytes)
                throw std::length_error("Candidate snapshot exceeds payload limit");
            bytes += size;
        }
        input::CandidatesChanged target;
        target.session = result.text_session;
        target.orientation = source.horizontal ? input::CandidateOrientation::horizontal
                                               : input::CandidateOrientation::vertical;
        if(source.selected_candidate >= 0) target.selected = static_cast<std::size_t>(source.selected_candidate);
        target.candidates.reserve(static_cast<std::size_t>(source.num_candidates));
        for(Sint32 index = 0; index < source.num_candidates; ++index)
            target.candidates.push_back(owned_text(source.candidates[index]));
        result.input.append(std::move(target));
        return;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        const auto direction = wheel_direction_sign(event.wheel.direction);
        if (direction.has_value()) {
            append_if_valid(result, ScrollInputEvent{
                event.wheel.x * *direction,
                event.wheel.y * *direction,
                to_logical_coordinate(event.wheel.mouse_x, metrics),
                to_logical_coordinate(event.wheel.mouse_y, metrics),
            });
        }
        return;
    }
    case SDL_EVENT_MOUSE_MOTION:
        append_if_valid(result, PointerInputEvent{
            PointerIdentity::mouse(),
            PointerAction::move,
            PointerButton::none,
            to_logical_coordinate(event.motion.x, metrics),
            to_logical_coordinate(event.motion.y, metrics),
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
                to_logical_coordinate(event.button.x, metrics),
                to_logical_coordinate(event.button.y, metrics),
            });
        }
        return;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED: {
        const auto action = map_touch_action(event.type);
        if (action.has_value()
                && metrics.logical_width() > 0.0F
                && metrics.logical_height() > 0.0F) {
            append_if_valid(result, PointerInputEvent{
                PointerIdentity::touch(event.tfinger.touchID, event.tfinger.fingerID),
                *action,
                button_for(*action),
                event.tfinger.x * metrics.logical_width(),
                event.tfinger.y * metrics.logical_height(),
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
            metrics.coordinate_width = event.window.data1;
            metrics.coordinate_height = event.window.data2;
            append_logical_resize(result, metrics);
        }
        return;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (event.window.data1 > 0 && event.window.data2 > 0) {
            metrics.pixel_width = event.window.data1;
            metrics.pixel_height = event.window.data2;
            append_logical_resize(result, metrics);
        }
        return;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        append_logical_resize(result, metrics);
        return;
    default:
        return;
    }
}

void SdlEventAdapter::merge(PlatformEvents& result, const SDL_Event& event, SdlWindowMetrics& metrics) {
    try { merge_event(result, event, metrics); }
    catch(...) {
        if(event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING
            || event.type == SDL_EVENT_TEXT_EDITING_CANDIDATES) ++result.rejected_text_events;
        throw;
    }
}

} // namespace ryn::detail
