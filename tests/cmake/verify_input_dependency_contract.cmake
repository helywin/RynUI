if(NOT DEFINED RYNUI_SOURCE_DIR)
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE public_headers
    "${RYNUI_SOURCE_DIR}/include/*.h"
    "${RYNUI_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE lower_layer_headers
    "${RYNUI_SOURCE_DIR}/src/reactive/*.h"
    "${RYNUI_SOURCE_DIR}/src/reactive/*.hpp"
    "${RYNUI_SOURCE_DIR}/src/layout/*.h"
    "${RYNUI_SOURCE_DIR}/src/layout/*.hpp"
    "${RYNUI_SOURCE_DIR}/src/component/*.h"
    "${RYNUI_SOURCE_DIR}/src/component/*.hpp")
list(APPEND contract_headers
    ${public_headers}
    ${lower_layer_headers}
    "${RYNUI_SOURCE_DIR}/src/input/platform_input.hpp")

foreach(contract_header IN LISTS contract_headers)
    file(READ "${contract_header}" header_contents)
    foreach(forbidden_pattern IN ITEMS
            "SDL"
            "Scancode"
            "PlatformWindowHandle"
            "PlatformGpuDeviceHandle"
            "GpuDeviceHandle")
        if(header_contents MATCHES "${forbidden_pattern}")
            message(FATAL_ERROR
                "Input dependency contract failed: ${contract_header} contains "
                "forbidden platform symbol ${forbidden_pattern}")
        endif()
    endforeach()
endforeach()

file(READ
    "${RYNUI_SOURCE_DIR}/src/platform/sdl/sdl_event_adapter.hpp"
    adapter_header)
if(NOT adapter_header MATCHES "SDL3/SDL_events\\.h")
    message(FATAL_ERROR "SDL event mapping is not encapsulated by the SDL adapter")
endif()

file(READ
    "${RYNUI_SOURCE_DIR}/src/input/platform_input.hpp"
    normalized_input_header)
foreach(required_pattern IN ITEMS
        "struct ScrollInputEvent"
        "float delta_x"
        "float delta_y")
    if(NOT normalized_input_header MATCHES "${required_pattern}")
        message(FATAL_ERROR
            "Normalized scroll input contract is missing ${required_pattern}")
    endif()
endforeach()

file(READ
    "${RYNUI_SOURCE_DIR}/src/platform/sdl/sdl_event_adapter.cpp"
    adapter_source)
foreach(required_pattern IN ITEMS
        "SDL_EVENT_MOUSE_WHEEL"
        "SDL_MOUSEWHEEL_NORMAL"
        "SDL_MOUSEWHEEL_FLIPPED"
        "event.wheel.mouse_x"
        "event.wheel.mouse_y")
    if(NOT adapter_source MATCHES "${required_pattern}")
        message(FATAL_ERROR
            "SDL wheel adapter contract is missing ${required_pattern}")
    endif()
endforeach()

message(STATUS "RynUI normalized input remains independent from SDL3 and backend handles")
