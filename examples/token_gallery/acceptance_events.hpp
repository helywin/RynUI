#pragma once
#include <SDL3/SDL.h>
#include <limits>

namespace rynui::example::acceptance {
inline constexpr Uint64 fixture_timestamp = std::numeric_limits<Uint64>::max();

inline bool resize_window(SDL_Window* window, int width, int height) {
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0) {
        if (!SDL_RestoreWindow(window) || !SDL_SyncWindow(window)) {
            return false;
        }
    }
    int actual_width{};
    int actual_height{};
    return SDL_SetWindowSize(window, width, height) && SDL_SyncWindow(window) &&
           SDL_GetWindowSize(window, &actual_width, &actual_height) && actual_width == width && actual_height == height;
}

class FixtureEventFilter final {
public:
    explicit FixtureEventFilter(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            SDL_SetEventFilter(
                [](void*, SDL_Event* event) {
                    const auto type = event->type;
                    const bool input = type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP ||
                                       type == SDL_EVENT_MOUSE_MOTION || type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                                       type == SDL_EVENT_MOUSE_BUTTON_UP || type == SDL_EVENT_MOUSE_WHEEL ||
                                       type == SDL_EVENT_WINDOW_FOCUS_LOST || type == SDL_EVENT_WINDOW_FOCUS_GAINED;
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
