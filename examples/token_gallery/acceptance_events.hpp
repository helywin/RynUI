#pragma once
#include <SDL3/SDL.h>
#include <limits>

namespace rynui::example::acceptance {
inline constexpr Uint64 fixture_timestamp = std::numeric_limits<Uint64>::max();

class FixtureEventFilter final {
public:
    explicit FixtureEventFilter(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            SDL_SetEventFilter(
                [](void*, SDL_Event* event) {
                    const auto type = event->type;
                    const bool input = type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP ||
                                       type == SDL_EVENT_MOUSE_MOTION || type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                                       type == SDL_EVENT_MOUSE_BUTTON_UP || type == SDL_EVENT_WINDOW_FOCUS_LOST ||
                                       type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                    return !input || event->common.timestamp == fixture_timestamp;
                },
                nullptr);
        }
    }

    ~FixtureEventFilter() {
        if (enabled_) {
            SDL_SetEventFilter(nullptr, nullptr);
        }
    }

private:
    bool enabled_{};
};
} // namespace rynui::example::acceptance
