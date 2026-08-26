find_package(Git REQUIRED)

add_executable(rynui_public_api_smoke public_api_smoke.cpp)
rynui_configure_cpp_target(rynui_public_api_smoke)
target_link_libraries(rynui_public_api_smoke PRIVATE RynUI::RynUI)

add_test(NAME rynui.public_api_smoke COMMAND rynui_public_api_smoke)

add_executable(rynui_string_tests string_tests.cpp)
rynui_configure_cpp_target(rynui_string_tests)
target_link_libraries(rynui_string_tests PRIVATE rynui_string)
add_test(NAME rynui.string COMMAND rynui_string_tests)

add_executable(
    rynui_font_dependency_contract_tests
    font_dependency_contract_tests.cpp
)
rynui_configure_cpp_target(rynui_font_dependency_contract_tests)
target_link_libraries(
    rynui_font_dependency_contract_tests
    PRIVATE RynUI::FreeType RynUI::HarfBuzz
)
target_compile_definitions(
    rynui_font_dependency_contract_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(
    NAME rynui.font_dependency_contract
    COMMAND rynui_font_dependency_contract_tests
)

add_executable(rynui_font_runtime_tests font_runtime_tests.cpp)
rynui_configure_cpp_target(rynui_font_runtime_tests)
target_link_libraries(rynui_font_runtime_tests PRIVATE rynui_font)
target_compile_definitions(
    rynui_font_runtime_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.font_runtime COMMAND rynui_font_runtime_tests)

add_executable(rynui_text_engine_tests text_engine_tests.cpp)
rynui_configure_cpp_target(rynui_text_engine_tests)
target_link_libraries(rynui_text_engine_tests PRIVATE rynui_text)
target_compile_definitions(
    rynui_text_engine_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_engine COMMAND rynui_text_engine_tests)

add_executable(rynui_text_state_tests text_state_tests.cpp)
rynui_configure_cpp_target(rynui_text_state_tests)
target_include_directories(rynui_text_state_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_text_state_tests
    PRIVATE rynui_text rynui_runtime rynui_reactive
)
target_compile_definitions(
    rynui_text_state_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_state COMMAND rynui_text_state_tests)

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

add_executable(rynui_reactive_allocation_benchmark reactive_allocation_benchmark.cpp)
rynui_configure_cpp_target(rynui_reactive_allocation_benchmark)
target_link_libraries(rynui_reactive_allocation_benchmark PRIVATE rynui_reactive)
add_test(NAME rynui.reactive_allocation COMMAND rynui_reactive_allocation_benchmark)

add_executable(rynui_node_store_tests node_store_tests.cpp)
rynui_configure_cpp_target(rynui_node_store_tests)
target_include_directories(rynui_node_store_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_node_store_tests PRIVATE rynui_runtime)
add_test(NAME rynui.node_store COMMAND rynui_node_store_tests)

add_executable(rynui_component_mount_tests component_mount_tests.cpp)
rynui_configure_cpp_target(rynui_component_mount_tests)
target_include_directories(rynui_component_mount_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_component_mount_tests PRIVATE rynui_runtime rynui_reactive)
add_test(NAME rynui.component_mount COMMAND rynui_component_mount_tests)

add_executable(rynui_layout_engine_tests layout_engine_tests.cpp)
rynui_configure_cpp_target(rynui_layout_engine_tests)
target_include_directories(rynui_layout_engine_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_layout_engine_tests PRIVATE rynui_layout)
add_test(NAME rynui.layout_engine COMMAND rynui_layout_engine_tests)

add_executable(rynui_dirty_queue_tests dirty_queue_tests.cpp)
rynui_configure_cpp_target(rynui_dirty_queue_tests)
target_include_directories(rynui_dirty_queue_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_dirty_queue_tests PRIVATE rynui_layout)
add_test(NAME rynui.dirty_queue COMMAND rynui_dirty_queue_tests)

add_executable(rynui_quad_primitive_tests quad_primitive_tests.cpp)
rynui_configure_cpp_target(rynui_quad_primitive_tests)
target_include_directories(rynui_quad_primitive_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_quad_primitive_tests PRIVATE rynui_graphics)
add_test(NAME rynui.quad_primitive COMMAND rynui_quad_primitive_tests)

add_executable(rynui_binding_quad_integration_tests binding_quad_integration_tests.cpp)
rynui_configure_cpp_target(rynui_binding_quad_integration_tests)
target_include_directories(rynui_binding_quad_integration_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_binding_quad_integration_tests
    PRIVATE rynui_graphics rynui_reactive
)
add_test(NAME rynui.binding_quad_integration COMMAND rynui_binding_quad_integration_tests)

add_executable(rynui_frame_scheduler_tests frame_scheduler_tests.cpp)
rynui_configure_cpp_target(rynui_frame_scheduler_tests)
target_include_directories(rynui_frame_scheduler_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_frame_scheduler_tests PRIVATE rynui_runtime)
add_test(NAME rynui.frame_scheduler COMMAND rynui_frame_scheduler_tests)

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
        "-DTEST_GIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependency_lock.cmake"
)

if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
    add_test(
        NAME rynui.text_dependency_options
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_CACHE_FILE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_text_dependency_options.cmake"
    )
endif()

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

foreach(text_contract_case IN ITEMS
        positive
        missing-package
        wrong-version
        missing-target
        missing-font)
    add_test(
        NAME "rynui.text_dependency_contract.${text_contract_case}"
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/text-contract-tests/${text_contract_case}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DTEST_CASE=${text_contract_case}"
            "-DTEST_LATIN_FONT=${RYNUI_VALIDATION_LATIN_FONT_FILE}"
            "-DTEST_CJK_FONT=${RYNUI_VALIDATION_CJK_FONT_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_text_dependency_contract.cmake"
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
            "-DTEST_LATIN_FONT=${RYNUI_VALIDATION_LATIN_FONT_FILE}"
            "-DTEST_CJK_FONT=${RYNUI_VALIDATION_CJK_FONT_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_shadercross_override.cmake"
    )
endif()
