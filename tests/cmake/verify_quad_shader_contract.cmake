cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SHADER_SOURCE OR SHADER_SOURCE STREQUAL "")
    message(FATAL_ERROR "SHADER_SOURCE is required")
endif()
if(NOT EXISTS "${SHADER_SOURCE}")
    message(FATAL_ERROR "Quad shader source is missing")
endif()

file(READ "${SHADER_SOURCE}" shader)

foreach(required IN ITEMS
        "register(b0, space1)"
        "viewportPixels"
        "pixelExtent"
        "radiusPixels"
        "centeredPixels"
        "guardNdc"
        "guardPixels"
        "all(pixelExtent > 0.0) ? 1.0 : 0.0"
        "* clipRect.zw + guardNdc"
        "smoothstep(-0.5, 0.5, distance)")
    string(FIND "${shader}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Quad shader is missing pixel-space rounded rectangle contract: ${required}")
    endif()
endforeach()

string(FIND "${shader}" "fwidth(distance)" derivative_width)
if(NOT derivative_width EQUAL -1)
    message(FATAL_ERROR "Quad shader must use the common fixed one-physical-pixel coverage width")
endif()

string(FIND
    "${shader}"
    "abs(input.uv - 0.5) - (0.5 - radius)"
    normalized_uv_distance)
if(NOT normalized_uv_distance EQUAL -1)
    message(FATAL_ERROR
        "Quad shader still computes rounded corners in aspect-distorted normalized UV space")
endif()
