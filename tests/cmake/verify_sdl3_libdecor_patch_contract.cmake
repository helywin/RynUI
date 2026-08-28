cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS TEST_SOURCE_DIR TEST_BINARY_DIR TEST_PATCH_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

set(fixture_dir "${TEST_BINARY_DIR}/fixture")
file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")
file(COPY "${TEST_SOURCE_DIR}/tests/fixtures/sdl3-3.4.14/"
    DESTINATION "${fixture_dir}")

include("${TEST_SOURCE_DIR}/cmake/dependencies/RynUIDependencyLock.cmake")
set(patch_file
    "${TEST_SOURCE_DIR}/cmake/patches/sdl3/0001-use-bundled-libdecor-resize-state.patch")
set(apply_command
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=${fixture_dir}"
    "-DPATCH_1=${patch_file}"
    "-DEXPECTED_PATCH_SHA256_1=${RYNUI_SDL3_LIBDECOR_PATCH_SHA256}"
    "-DEXPECTED_VERSION_FILE=CMakeLists.txt"
    "-DEXPECTED_VERSION_PATTERN=VERSION \"3[.]4[.]14\""
    "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
    -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake")

execute_process(
    COMMAND ${apply_command}
    RESULT_VARIABLE apply_result
    OUTPUT_VARIABLE apply_output
    ERROR_VARIABLE apply_error)
if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR "SDL3 libdecor patch failed:\n${apply_output}${apply_error}")
endif()

file(READ "${fixture_dir}/cmake/sdlchecks.cmake" patched_checks)
file(READ "${fixture_dir}/src/video/wayland/SDL_waylandwindow.c" patched_window)
foreach(pattern IN ITEMS
        "if\\(SDL_RYNUI_LIBDECOR_TARGET\\)"
        "sdl_link_dependency\\(libdecor LIBS"
        "SDL_LIBDECOR_HAS_RESIZING_STATE=1")
    if(NOT patched_checks MATCHES "${pattern}")
        message(FATAL_ERROR "Patched SDL3 dependency path is missing: ${pattern}")
    endif()
endforeach()
if(NOT patched_window MATCHES
        "#ifdef SDL_LIBDECOR_HAS_RESIZING_STATE[^#]+LIBDECOR_WINDOW_STATE_RESIZING[^#]+#endif[^#]+#if SDL_LIBDECOR_CHECK_VERSION\\(0, 3, 0\\)[^#]+LIBDECOR_WINDOW_STATE_CONSTRAINED_LEFT")
    message(FATAL_ERROR
        "SDL3 resize state must be enabled separately from untouched 0.3-only APIs.")
endif()

execute_process(
    COMMAND ${apply_command}
    RESULT_VARIABLE repeat_result
    OUTPUT_QUIET ERROR_QUIET)
if(repeat_result EQUAL 0)
    message(FATAL_ERROR "Repeated SDL3 patch application must fail in strict mode.")
endif()

set(wrong_fixture "${TEST_BINARY_DIR}/wrong-version")
file(COPY "${TEST_SOURCE_DIR}/tests/fixtures/sdl3-3.4.14/"
    DESTINATION "${wrong_fixture}")
file(READ "${wrong_fixture}/CMakeLists.txt" wrong_version_text)
string(REPLACE "3.4.14" "3.4.13" wrong_version_text "${wrong_version_text}")
file(WRITE "${wrong_fixture}/CMakeLists.txt" "${wrong_version_text}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${wrong_fixture}"
        "-DPATCH_1=${patch_file}"
        "-DEXPECTED_VERSION_FILE=CMakeLists.txt"
        "-DEXPECTED_VERSION_PATTERN=VERSION \"3[.]4[.]14\""
        "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
        -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake"
    RESULT_VARIABLE wrong_version_result
    OUTPUT_QUIET ERROR_QUIET)
if(wrong_version_result EQUAL 0)
    message(FATAL_ERROR "SDL3 patch must reject a wrong source version.")
endif()

message(STATUS "SDL3 libdecor patch is strict and preserves other 0.3 gates")
