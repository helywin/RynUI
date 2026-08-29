cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS TEST_BINARY_DIR TEST_CONFIG)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

set(build_graph "${TEST_BINARY_DIR}/CMakeFiles/impl-${TEST_CONFIG}.ninja")
if(NOT EXISTS "${build_graph}")
    message(FATAL_ERROR "Generated Windows build graph does not exist: ${build_graph}")
endif()
file(READ "${build_graph}" graph_text)

foreach(forbidden IN ITEMS
        "rynui_libdecor_external"
        "_deps/rynui-libdecor-stage"
        "_deps/rynui-libdecor-build"
        "SDL_LIBDECOR_HAS_RESIZING_STATE"
        "libdecor-0[.]so"
        "libdecor-cairo[.]so")
    if(graph_text MATCHES "${forbidden}")
        message(FATAL_ERROR "Windows build graph contains libdecor input: ${forbidden}")
    endif()
endforeach()

file(GLOB libdecor_build_entries "${TEST_BINARY_DIR}/_deps/rynui-libdecor-*")
if(libdecor_build_entries)
    message(FATAL_ERROR
        "Windows build tree contains libdecor source/build/staging entries: ${libdecor_build_entries}")
endif()

message(STATUS "Windows build graph and build tree contain no libdecor dependency")
