add_executable(rynui_public_api_smoke public_api_smoke.cpp)
rynui_configure_cpp_target(rynui_public_api_smoke)
target_link_libraries(rynui_public_api_smoke PRIVATE RynUI::RynUI)

add_test(NAME rynui.public_api_smoke COMMAND rynui_public_api_smoke)

add_test(
    NAME rynui.dependency_lock
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependency_lock.cmake"
)

foreach(contract_case IN ITEMS invalid-mode missing-target)
    add_test(
        NAME "rynui.dependency_contract.${contract_case}"
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/${contract_case}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DTEST_EXPECTED_TOOLCHAIN=${RYNUI_EXPECTED_TOOLCHAIN}"
            "-DTEST_CASE=${contract_case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_configure_failure.cmake"
    )
endforeach()
