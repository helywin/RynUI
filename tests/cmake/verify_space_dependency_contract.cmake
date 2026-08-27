cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR "${RYNUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

set(space_header "${RYNUI_SOURCE_DIR}/include/ryn/space.hpp")
if(NOT EXISTS "${space_header}")
    message(FATAL_ERROR "Public Space header is missing")
endif()

file(READ "${space_header}" space_source)
foreach(forbidden IN ITEMS
        "SDL"
        "runtime/"
        "graphics/"
        "renderer/"
        "font/"
        "harfbuzz"
        "freetype"
        "NodeId"
        "ComponentId"
        "LayoutEngine"
        "ThemeSnapshot"
        "InteractionId"
        "Scene"
        "GPU"
        "Primitive"
        "Shader")
    string(FIND "${space_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Public Space header leaked forbidden dependency token: ${forbidden}")
    endif()
endforeach()
