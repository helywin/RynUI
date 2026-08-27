cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SHADER_SOURCE OR SHADER_SOURCE STREQUAL "")
    message(FATAL_ERROR "SHADER_SOURCE is required")
endif()
if(NOT EXISTS "${SHADER_SOURCE}")
    message(FATAL_ERROR "Quad shader source is missing")
endif()

file(READ "${SHADER_SOURCE}" shader)

foreach(required IN ITEMS
        "fwidth(input.uv)"
        "pixelExtent"
        "radiusPixels"
        "centeredPixels"
        "fwidth(distance)")
    string(FIND "${shader}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Quad shader is missing pixel-space rounded rectangle contract: ${required}")
    endif()
endforeach()

string(FIND
    "${shader}"
    "abs(input.uv - 0.5) - (0.5 - radius)"
    normalized_uv_distance)
if(NOT normalized_uv_distance EQUAL -1)
    message(FATAL_ERROR
        "Quad shader still computes rounded corners in aspect-distorted normalized UV space")
endif()
