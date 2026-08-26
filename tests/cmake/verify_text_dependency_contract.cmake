cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        TEST_SOURCE_DIR
        TEST_BINARY_DIR
        TEST_GENERATOR
        TEST_CXX_COMPILER
        TEST_CASE
        TEST_LATIN_FONT
        TEST_CJK_FONT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")

set(fixture_root "${TEST_SOURCE_DIR}/tests/cmake")
set(configure_arguments
    -S "${fixture_root}/text-dependency-contract"
    -B "${TEST_BINARY_DIR}"
    -G "${TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    "-DRYNUI_ROOT=${TEST_SOURCE_DIR}"
    -DRYNUI_DEPENDENCY_MODE=SYSTEM
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DFreetype_DIR=${fixture_root}/fake-freetype"
    "-Dharfbuzz_DIR=${fixture_root}/fake-harfbuzz"
    "-DRYNUI_SYSTEM_LATIN_FONT_FILE=${TEST_LATIN_FONT}"
    "-DRYNUI_SYSTEM_CJK_FONT_FILE=${TEST_CJK_FONT}"
)

set(expect_success FALSE)
if(TEST_CASE STREQUAL "positive")
    set(expect_success TRUE)
elseif(TEST_CASE STREQUAL "missing-package")
    list(APPEND configure_arguments -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=TRUE)
    set(expected_diagnostic "disabled")
elseif(TEST_CASE STREQUAL "wrong-version")
    list(APPEND configure_arguments
        "-DFreetype_DIR=${fixture_root}/fake-freetype-wrong-version")
    set(expected_diagnostic "2.14.3")
elseif(TEST_CASE STREQUAL "missing-target")
    list(APPEND configure_arguments
        "-DFreetype_DIR=${fixture_root}/fake-freetype-no-target")
    set(expected_diagnostic "did not define required target")
elseif(TEST_CASE STREQUAL "missing-font")
    list(APPEND configure_arguments -DRYNUI_SYSTEM_LATIN_FONT_FILE=)
    set(expected_diagnostic "requires RYNUI_SYSTEM_LATIN_FONT_FILE")
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

if(expect_success)
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR
            "SYSTEM text dependency contract configure failed.\n${configure_output}")
    endif()
elseif(configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configure unexpectedly succeeded for ${TEST_CASE}.\n${configure_output}")
elseif(NOT configure_output MATCHES "${expected_diagnostic}")
    message(FATAL_ERROR
        "Configure failed without expected diagnostic '${expected_diagnostic}'.\n"
        "${configure_output}")
endif()
