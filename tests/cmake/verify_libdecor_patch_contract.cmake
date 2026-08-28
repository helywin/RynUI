cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS TEST_SOURCE_DIR TEST_BINARY_DIR TEST_PATCH_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

set(fixture_root "${TEST_BINARY_DIR}/fixture")
file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(COPY "${TEST_SOURCE_DIR}/tests/fixtures/libdecor-0.2.5/"
    DESTINATION "${fixture_root}")

set(resizing_patch
    "${TEST_SOURCE_DIR}/cmake/patches/libdecor/0001-expose-resizing-state.patch")
set(configuration_patch
    "${TEST_SOURCE_DIR}/cmake/patches/libdecor/0002-retain-configuration.patch")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${fixture_root}"
        "-DPATCH_1=${resizing_patch}"
        "-DPATCH_2=${configuration_patch}"
        "-DEXPECTED_VERSION_FILE=meson.build"
        "-DEXPECTED_VERSION_PATTERN=version: '0[.]2[.]5'"
        "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
        -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake"
    RESULT_VARIABLE apply_result
    OUTPUT_VARIABLE apply_output
    ERROR_VARIABLE apply_error
)
if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR "Ordered libdecor patches failed:\n${apply_output}${apply_error}")
endif()

file(READ "${fixture_root}/src/libdecor.h" patched_header)
file(READ "${fixture_root}/src/libdecor.c" patched_source)
file(READ "${fixture_root}/meson.build" patched_meson)
foreach(pattern IN ITEMS
        "LIBDECOR_WINDOW_STATE_RESIZING = 1 << 8"
        "libdecor_configuration_ref"
        "libdecor_configuration_unref")
    if(NOT patched_header MATCHES "${pattern}" AND NOT patched_source MATCHES "${pattern}")
        message(FATAL_ERROR "Patched fixture is missing: ${pattern}")
    endif()
endforeach()
if(NOT patched_meson MATCHES "version: '0[.]2[.]5'")
    message(FATAL_ERROR "libdecor patch must not change the advertised version.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${fixture_root}"
        "-DPATCH_1=${resizing_patch}"
        "-DPATCH_2=${configuration_patch}"
        "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
        -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake"
    RESULT_VARIABLE repeated_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(repeated_result EQUAL 0)
    message(FATAL_ERROR "Applying the dependency patches twice must fail-fast.")
endif()

set(wrong_version_root "${TEST_BINARY_DIR}/wrong-version")
file(COPY "${TEST_SOURCE_DIR}/tests/fixtures/libdecor-0.2.5/"
    DESTINATION "${wrong_version_root}")
file(READ "${wrong_version_root}/meson.build" wrong_version_meson)
string(REPLACE "version: '0.2.5'" "version: '0.2.4'"
    wrong_version_meson "${wrong_version_meson}")
file(WRITE "${wrong_version_root}/meson.build" "${wrong_version_meson}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${wrong_version_root}"
        "-DPATCH_1=${resizing_patch}"
        "-DPATCH_2=${configuration_patch}"
        "-DEXPECTED_VERSION_FILE=meson.build"
        "-DEXPECTED_VERSION_PATTERN=version: '0[.]2[.]5'"
        "-DPATCH_EXECUTABLE=${TEST_PATCH_EXECUTABLE}"
        -P "${TEST_SOURCE_DIR}/cmake/ApplySourcePatches.cmake"
    RESULT_VARIABLE wrong_version_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(wrong_version_result EQUAL 0)
    message(FATAL_ERROR "A wrong libdecor source version must fail-fast.")
endif()

message(STATUS
    "libdecor patches apply in order and reject repetition or a wrong source version")
