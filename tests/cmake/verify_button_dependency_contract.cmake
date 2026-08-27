cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR "${RYNUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

set(button_header "${RYNUI_SOURCE_DIR}/include/ryn/button.hpp")
if(NOT EXISTS "${button_header}")
    message(FATAL_ERROR "Public Button header is missing")
endif()

file(READ "${button_header}" button_source)
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
        "InteractionId"
        "Scene"
        "Primitive"
        "Shader")
    string(FIND "${button_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Public Button header leaked forbidden dependency token: ${forbidden}")
    endif()
endforeach()
