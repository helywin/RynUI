cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        TEST_SOURCE_DIR
        TEST_BINARY_DIR
        TEST_GENERATOR
        TEST_CXX_COMPILER
        TEST_CASE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")

set(configure_arguments
    -S "${TEST_SOURCE_DIR}"
    -B "${TEST_BINARY_DIR}"
    -G "${TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    "-DRYNUI_EXPECTED_TOOLCHAIN=${TEST_EXPECTED_TOOLCHAIN}"
    -DBUILD_TESTING=OFF
    -DRYNUI_BUILD_EXAMPLES=OFF
)

if(TEST_CASE STREQUAL "invalid-mode")
    list(APPEND configure_arguments -DRYNUI_DEPENDENCY_MODE=AUTO)
    set(expected_diagnostic "RYNUI_DEPENDENCY_MODE must be one of")
elseif(TEST_CASE STREQUAL "missing-target")
    list(APPEND configure_arguments
        -DRYNUI_DEPENDENCY_MODE=SYSTEM
        "-DSDL3_DIR=${TEST_SOURCE_DIR}/tests/cmake/fake-sdl3-no-target")
    set(expected_diagnostic "did not define required target SDL3::SDL3")
else()
    message(FATAL_ERROR "Unknown TEST_CASE: ${TEST_CASE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${configure_arguments}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

set(configure_output "${configure_stdout}\n${configure_stderr}")
if(configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configure unexpectedly succeeded for ${TEST_CASE}.\n${configure_output}")
endif()

if(NOT configure_output MATCHES "${expected_diagnostic}")
    message(FATAL_ERROR
        "Configure failed without the expected diagnostic '${expected_diagnostic}'.\n"
        "${configure_output}")
endif()
