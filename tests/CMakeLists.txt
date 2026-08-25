add_executable(rynui_public_api_smoke public_api_smoke.cpp)
rynui_configure_cpp_target(rynui_public_api_smoke)
target_link_libraries(rynui_public_api_smoke PRIVATE RynUI::RynUI)

add_test(NAME rynui.public_api_smoke COMMAND rynui_public_api_smoke)

add_executable(rynui_signal_tests signal_tests.cpp)
rynui_configure_cpp_target(rynui_signal_tests)
target_link_libraries(rynui_signal_tests PRIVATE rynui_reactive)
add_test(NAME rynui.signal COMMAND rynui_signal_tests)

add_executable(rynui_memo_tests memo_tests.cpp)
rynui_configure_cpp_target(rynui_memo_tests)
target_link_libraries(rynui_memo_tests PRIVATE rynui_reactive)
add_test(NAME rynui.memo COMMAND rynui_memo_tests)

add_executable(rynui_batch_tests batch_tests.cpp)
rynui_configure_cpp_target(rynui_batch_tests)
target_link_libraries(rynui_batch_tests PRIVATE rynui_reactive)
add_test(NAME rynui.batch COMMAND rynui_batch_tests)

add_executable(rynui_scope_effect_tests scope_effect_tests.cpp)
rynui_configure_cpp_target(rynui_scope_effect_tests)
target_link_libraries(rynui_scope_effect_tests PRIVATE rynui_reactive)
add_test(NAME rynui.scope_effect COMMAND rynui_scope_effect_tests)

add_executable(rynui_platform_lifecycle_tests platform_lifecycle_tests.cpp)
rynui_configure_cpp_target(rynui_platform_lifecycle_tests)
target_include_directories(rynui_platform_lifecycle_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_platform_lifecycle_tests PRIVATE rynui_platform_sdl)
add_test(NAME rynui.platform_lifecycle COMMAND rynui_platform_lifecycle_tests)

add_executable(rynui_frame_renderer_tests frame_renderer_tests.cpp)
rynui_configure_cpp_target(rynui_frame_renderer_tests)
target_link_libraries(rynui_frame_renderer_tests PRIVATE rynui_renderer_sdl)
add_test(NAME rynui.frame_renderer COMMAND rynui_frame_renderer_tests)

add_test(
    NAME rynui.generated_shaders
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_generated_shaders.cmake"
)

add_test(
    NAME rynui.dependency_lock
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependency_lock.cmake"
)

foreach(contract_case IN ITEMS
        invalid-mode
        missing-target
        missing-shadercross
        invalid-shadercross
        wrong-shadercross
        cross-missing-shadercross)
    add_test(
        NAME "rynui.dependency_contract.${contract_case}"
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/${contract_case}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DTEST_EXPECTED_TOOLCHAIN=${RYNUI_EXPECTED_TOOLCHAIN}"
            "-DTEST_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DTEST_CMAKE_COMMAND=${CMAKE_COMMAND}"
            "-DTEST_CASE=${contract_case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_configure_failure.cmake"
    )
endforeach()

if(TARGET shadercross)
    add_test(
        NAME rynui.shadercross_override
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/shadercross-override"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DTEST_EXPECTED_TOOLCHAIN=${RYNUI_EXPECTED_TOOLCHAIN}"
            "-DTEST_SHADERCROSS_EXECUTABLE=$<TARGET_FILE:shadercross>"
            "-DTEST_CONFIGURATION=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_shadercross_override.cmake"
    )
endif()
