cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS
        TEST_SOURCE_DIR TEST_BINARY_DIR TEST_GENERATOR TEST_C_COMPILER TEST_CASE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required.")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")
set(contract_source
    "${TEST_SOURCE_DIR}/tests/cmake/libdecor-resolver-contract")
set(contract_build "${TEST_BINARY_DIR}/build")
set(configure_command
    "${CMAKE_COMMAND}"
    -S "${contract_source}"
    -B "${contract_build}"
    -G "${TEST_GENERATOR}"
    "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}"
    "-DTEST_SOURCE_DIR=${TEST_SOURCE_DIR}")

set(expect_success FALSE)
if(TEST_CASE STREQUAL "positive")
    set(source_override "${TEST_BINARY_DIR}/libdecor-source")
    file(COPY "${TEST_SOURCE_DIR}/tests/fixtures/libdecor-0.2.5/"
        DESTINATION "${source_override}")
    list(APPEND configure_command
        "-DTEST_CASE=${TEST_CASE}"
        "-DFETCHCONTENT_SOURCE_DIR_RYNUI_LIBDECOR=${source_override}")
    set(expect_success TRUE)
elseif(TEST_CASE STREQUAL "system" OR TEST_CASE STREQUAL "non-linux")
    list(APPEND configure_command "-DTEST_CASE=${TEST_CASE}")
    set(expect_success TRUE)
elseif(TEST_CASE STREQUAL "missing-meson")
    list(APPEND configure_command "-DTEST_CASE=${TEST_CASE}"
        "-DRYNUI_MESON_EXECUTABLE=${TEST_BINARY_DIR}/missing-meson")
    set(expected_diagnostic "requires a runnable RYNUI_MESON_EXECUTABLE")
elseif(TEST_CASE STREQUAL "missing-package")
    file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/empty-pkgconfig")
    set(configure_command
        "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_LIBDIR=${TEST_BINARY_DIR}/empty-pkgconfig"
        ${configure_command} "-DTEST_CASE=${TEST_CASE}")
    set(expected_diagnostic "required package")
else()
    message(FATAL_ERROR "Unknown TEST_CASE: ${TEST_CASE}")
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
set(combined_output "${configure_output}${configure_error}")

if(expect_success)
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR
            "Positive libdecor resolver configure failed:\n${combined_output}")
    endif()
elseif(configure_result EQUAL 0)
    message(FATAL_ERROR "Expected libdecor resolver configure failure for ${TEST_CASE}.")
elseif(NOT combined_output MATCHES "${expected_diagnostic}")
    message(FATAL_ERROR
        "Configure failure did not contain '${expected_diagnostic}':\n${combined_output}")
endif()

message(STATUS "libdecor resolver contract passed: ${TEST_CASE}")
