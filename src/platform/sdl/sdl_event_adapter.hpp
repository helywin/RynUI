#pragma once

#include "platform/sdl/platform_state.hpp"

#include <SDL3/SDL_events.h>

namespace ryn::detail {

struct SdlWindowMetrics {
    int width{0};
    int height{0};
};

class SdlEventAdapter final {
public:
    static void merge(
        PlatformEvents& result,
        const SDL_Event& event,
        SdlWindowMetrics& metrics);
};

} // namespace ryn::detail
