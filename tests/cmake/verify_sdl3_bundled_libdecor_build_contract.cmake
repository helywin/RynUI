cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS TEST_BINARY_DIR TEST_CONFIG)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

set(build_graph "${TEST_BINARY_DIR}/CMakeFiles/impl-${TEST_CONFIG}.ninja")
if(NOT EXISTS "${build_graph}")
    message(FATAL_ERROR "Generated build graph does not exist: ${build_graph}")
endif()
file(READ "${build_graph}" graph_text)

foreach(pattern IN ITEMS
        "SDL_LIBDECOR_HAS_RESIZING_STATE=1"
        "_deps/rynui-libdecor-stage/include/libdecor-0"
        "_deps/rynui-libdecor-stage/lib/libdecor-0[.]so"
        "rynui_libdecor_external")
    if(NOT graph_text MATCHES "${pattern}")
        message(FATAL_ERROR "Bundled SDL/libdecor build graph is missing: ${pattern}")
    endif()
endforeach()
if(graph_text MATCHES "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR")
    message(FATAL_ERROR "Bundled SDL must not dynamically discover system libdecor.")
endif()
if(graph_text MATCHES "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC=")
    message(FATAL_ERROR
        "Bundled SDL must not use dynamic Wayland symbol lookup with direct libdecor.")
endif()

message(STATUS "SDL compiles and links only against staged patched libdecor")
