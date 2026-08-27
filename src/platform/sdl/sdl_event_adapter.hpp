#pragma once

#include "platform/sdl/platform_state.hpp"

#include <SDL3/SDL_events.h>

namespace ryn::detail {

using SdlWindowMetrics = PlatformWindowMetrics;

class SdlEventAdapter final {
public:
    static void merge(
        PlatformEvents& result,
        const SDL_Event& event,
        SdlWindowMetrics& metrics);
};

} // namespace ryn::detail
