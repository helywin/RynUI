cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        TEST_SOURCE_DIR
        TEST_BINARY_DIR
        TEST_GENERATOR
        TEST_CXX_COMPILER
        TEST_SHADERCROSS_EXECUTABLE
        TEST_CONFIGURATION
        TEST_LATIN_FONT
        TEST_CJK_FONT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${TEST_SOURCE_DIR}"
        -B "${TEST_BINARY_DIR}"
        -G "${TEST_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
        "-DRYNUI_EXPECTED_TOOLCHAIN=${TEST_EXPECTED_TOOLCHAIN}"
        -DRYNUI_DEPENDENCY_MODE=SYSTEM
        "-DSDL3_DIR=${TEST_SOURCE_DIR}/tests/cmake/fake-sdl3"
        "-DFreetype_DIR=${TEST_SOURCE_DIR}/tests/cmake/fake-freetype"
        "-Dharfbuzz_DIR=${TEST_SOURCE_DIR}/tests/cmake/fake-harfbuzz"
        "-DRYNUI_SHADERCROSS_EXECUTABLE=${TEST_SHADERCROSS_EXECUTABLE}"
        "-DRYNUI_SYSTEM_LATIN_FONT_FILE=${TEST_LATIN_FONT}"
        "-DRYNUI_SYSTEM_CJK_FONT_FILE=${TEST_CJK_FONT}"
        -DBUILD_TESTING=OFF
        -DRYNUI_BUILD_EXAMPLES=ON
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configure with host tool override failed.\n${configure_stdout}\n${configure_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${TEST_BINARY_DIR}"
        --config "${TEST_CONFIGURATION}" --target rynui_shaders
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Shader build with host tool override failed.\n${build_stdout}\n${build_stderr}")
endif()

set(SHADER_DIRECTORY "${TEST_BINARY_DIR}/generated/shaders")
include("${TEST_SOURCE_DIR}/tests/cmake/verify_generated_shaders.cmake")
