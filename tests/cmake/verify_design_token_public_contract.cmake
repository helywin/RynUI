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

foreach(contract_case IN ITEMS css-string arbitrary-key invalid-constexpr)
    set(case_binary_dir "${TEST_BINARY_DIR}/${contract_case}")
    file(REMOVE_RECURSE "${case_binary_dir}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${TEST_SOURCE_DIR}/tests/cmake/design-token-public-contract"
            -B "${case_binary_dir}"
            -G "${TEST_GENERATOR}"
            "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
            "-DRYNUI_ROOT=${TEST_SOURCE_DIR}"
            "-DDESIGN_TOKEN_CONTRACT_CASE=${contract_case}"
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_stdout
        ERROR_VARIABLE configure_stderr
    )
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR
            "Design Token ${contract_case} contract failed to configure.\n"
            "${configure_stdout}\n${configure_stderr}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${case_binary_dir}"
            --config "${TEST_CONFIGURATION}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_stdout
        ERROR_VARIABLE build_stderr
    )
    if(build_result EQUAL 0)
        message(FATAL_ERROR
            "Design Token ${contract_case} unexpectedly compiled; token values must "
            "reject CSS strings, arbitrary keys, and invalid constant values.")
    endif()
endforeach()
