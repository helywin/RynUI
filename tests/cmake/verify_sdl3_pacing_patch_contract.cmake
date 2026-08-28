cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS TEST_SOURCE_DIR TEST_BINARY_DIR TEST_BUILD_DIR TEST_PATCH_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

file(GLOB_RECURSE sdl_archives
    "${TEST_BUILD_DIR}/_deps/sdl3-subbuild/*/SDL3-3.4.14.tar.gz")
list(LENGTH sdl_archives archive_count)
if(NOT archive_count EQUAL 1)
    message(FATAL_ERROR
        "Expected one locked SDL3 archive in the build tree, found ${archive_count}.")
endif()
list(GET sdl_archives 0 sdl_archive)

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/source")
file(ARCHIVE_EXTRACT INPUT "${sdl_archive}"
    DESTINATION "${TEST_BINARY_DIR}/source")
set(source_dir "${TEST_BINARY_DIR}/source/SDL3-3.4.14")

include("${TEST_SOURCE_DIR}/cmake/dependencies/RynUIDependencyLock.cmake")
set(patch_1
    "${TEST_SOURCE_DIR}/cmake/patches/sdl3/0001-use-bundled-libdecor-resize-state.patch")
set(patch_2
    "${TEST_SOURCE_DIR}/cmake/patches/sdl3/0002-pace-libdecor-resize-configures.patch")
set(apply_command
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=${source_dir}"
    "-DPATCH_1=${patch_1}"
    "-DPATCH_2=${patch_2}"
    "-DEXPECTED_PATCH_SHA256_1=${RYNUI_SDL3_LIBDECOR_PATCH_SHA256}"
    "-DEXPECTED_PATCH_SHA256_2=${RYNUI_SDL3_LIBDECOR_PACING_PATCH_SHA256}"
    "-DEXPECTED_VERSION_FILE=CMakeLists.txt"
    "-DEXPECTED_VERSION_PATTERN=VERSION \"3[.]4[.]14\""
    "-DEXPECTED_SOURCE_FILE_1=cmake/sdlchecks.cmake"
    "-DEXPECTED_SOURCE_SHA256_1=${RYNUI_SDL3_SDLCHECKS_SHA256}"
    "-DEXPECTED_SOURCE_FILE_2=src/video/wayland/SDL_waylandwindow.c"
    "-DEXPECTED_SOURCE_SHA256_2=${RYNUI_SDL3_WAYLAND_WINDOW_SOURCE_SHA256}"
    "-DEXPECTED_SOURCE_FILE_3=src/video/wayland/SDL_waylandwindow.h"
    "-DEXPECTED_SOURCE_SHA256_3=${RYNUI_SDL3_WAYLAND_WINDOW_HEADER_SHA256}"
    "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
    -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake")

execute_process(
    COMMAND ${apply_command}
    RESULT_VARIABLE apply_result
    OUTPUT_VARIABLE apply_output
    ERROR_VARIABLE apply_error)
if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR "SDL3 pacing patches failed:\n${apply_output}${apply_error}")
endif()

file(READ "${source_dir}/src/video/wayland/SDL_waylandwindow.c" window_source)
file(READ "${source_dir}/src/video/wayland/SDL_waylandwindow.h" window_header)
foreach(pattern IN ITEMS
        "pending_resize_configuration"
        "libdecor_configuration_ref\\(configuration\\)"
        "CommitLibdecorConfiguration\\(wind, configuration\\)"
        "ReleasePendingLibdecorConfiguration\\(wind\\)"
        "libdecor resize pacing: received=")
    if(NOT window_source MATCHES "${pattern}" AND NOT window_header MATCHES "${pattern}")
        message(FATAL_ERROR "SDL3 pacing source is missing: ${pattern}")
    endif()
endforeach()
file(READ "${patch_2}" pacing_patch_text)
if(pacing_patch_text MATCHES
        "focus_count|focus_changed|keyboard_focus|pointer_focus")
    message(FATAL_ERROR "Focus events must not advance resize pacing.")
endif()

execute_process(
    COMMAND ${apply_command}
    RESULT_VARIABLE repeat_result
    OUTPUT_QUIET ERROR_QUIET)
if(repeat_result EQUAL 0)
    message(FATAL_ERROR "Repeated SDL3 pacing patch application must fail.")
endif()

message(STATUS "SDL3 pacing patches apply to the locked pristine archive")
