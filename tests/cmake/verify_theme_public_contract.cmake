cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        TEST_SOURCE_DIR
        TEST_BINARY_DIR
        TEST_GENERATOR
        TEST_CXX_COMPILER
        TEST_CONFIGURATION)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${TEST_SOURCE_DIR}/tests/cmake/theme-public-contract"
        -B "${TEST_BINARY_DIR}"
        -G "${TEST_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
        "-DRYNUI_ROOT=${TEST_SOURCE_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Theme public contract failed to configure.\n"
        "${configure_stdout}\n${configure_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${TEST_BINARY_DIR}"
        --config "${TEST_CONFIGURATION}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
if(build_result EQUAL 0)
    message(FATAL_ERROR
        "ThemeProps accepted an arbitrary string config; only typed ThemeConfig/Prop is allowed.")
endif()
