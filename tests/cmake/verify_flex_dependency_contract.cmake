cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR "${RYNUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

set(flex_header "${RYNUI_SOURCE_DIR}/include/ryn/flex.hpp")
if(NOT EXISTS "${flex_header}")
    message(FATAL_ERROR "Public Flex header is missing")
endif()

file(READ "${flex_header}" flex_source)
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
    string(FIND "${flex_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Public Flex header leaked forbidden dependency token: ${forbidden}")
    endif()
endforeach()
