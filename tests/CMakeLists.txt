find_package(Git REQUIRED)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME rynui.libdecor_dependency_contract
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_libdecor_dependency_contract.cmake"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(RYNUI_TEST_PATCH_EXECUTABLE patch REQUIRED)

    add_test(
        NAME rynui.libdecor_patch_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/libdecor-patches"
            "-DTEST_PATCH_EXECUTABLE=${RYNUI_TEST_PATCH_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_libdecor_patch_contract.cmake"
    )

    add_test(
        NAME rynui.sdl3_libdecor_patch_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/sdl3-libdecor-patch"
            "-DTEST_PATCH_EXECUTABLE=${RYNUI_TEST_PATCH_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_sdl3_libdecor_patch_contract.cmake"
    )

    add_test(
        NAME rynui.sdl3_bundled_libdecor_build_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DTEST_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_sdl3_bundled_libdecor_build_contract.cmake"
    )

    add_test(
        NAME rynui.sdl3_pacing_patch_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/sdl3-pacing-patch"
            "-DTEST_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DTEST_PATCH_EXECUTABLE=${RYNUI_TEST_PATCH_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_sdl3_pacing_patch_contract.cmake"
    )

    add_executable(rynui_wayland_resize_pacing_tests wayland_resize_pacing_tests.cpp)
    rynui_configure_cpp_target(rynui_wayland_resize_pacing_tests)
    add_test(NAME rynui.wayland_resize_pacing COMMAND rynui_wayland_resize_pacing_tests)

    set(libdecor_contract_cases
        positive system non-linux missing-meson missing-package)
else()
    set(libdecor_contract_cases non-linux)
endif()

foreach(libdecor_contract_case IN LISTS libdecor_contract_cases)
    add_test(
        NAME "rynui.libdecor_resolver_contract.${libdecor_contract_case}"
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/libdecor-resolver-${libdecor_contract_case}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DTEST_CASE=${libdecor_contract_case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_libdecor_resolver_contract.cmake"
    )
endforeach()

if(WIN32)
    if(TARGET RynUI::LibDecor OR TARGET rynui_libdecor_external)
        message(FATAL_ERROR "Windows configure must not create libdecor targets.")
    endif()
    add_test(
        NAME rynui.windows_libdecor_isolation
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DTEST_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_libdecor_isolation.cmake"
    )
    add_test(
        NAME rynui.windows_libdecor_evidence
        COMMAND ${CMAKE_COMMAND}
            "-DEVIDENCE_FILE=${PROJECT_SOURCE_DIR}/openspec/changes/007-20260828-fix-linux-wayland-libdecor-resize/evidence/windows-dependency-isolation.md"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_libdecor_evidence.cmake"
    )
endif()

add_test(
    NAME rynui.design_token_catalog
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/update_ant_design_tokens.py"
        --repo-root "${PROJECT_SOURCE_DIR}"
        --verify-repository
        --self-test
)

add_executable(rynui_public_api_smoke public_api_smoke.cpp)
rynui_configure_cpp_target(rynui_public_api_smoke)
target_link_libraries(rynui_public_api_smoke PRIVATE RynUI::RynUI)

add_test(NAME rynui.public_api_smoke COMMAND rynui_public_api_smoke)

add_executable(rynui_design_token_tests design_token_tests.cpp)
rynui_configure_cpp_target(rynui_design_token_tests)
target_link_libraries(rynui_design_token_tests PRIVATE rynui_theme)
add_test(NAME rynui.design_token COMMAND rynui_design_token_tests)

add_executable(rynui_animation_time_tests animation_time_tests.cpp)
rynui_configure_cpp_target(rynui_animation_time_tests)
target_link_libraries(rynui_animation_time_tests PRIVATE rynui_animation)
add_test(NAME rynui.animation_time COMMAND rynui_animation_time_tests)

add_executable(rynui_animation_value_tests animation_value_tests.cpp)
rynui_configure_cpp_target(rynui_animation_value_tests)
target_link_libraries(rynui_animation_value_tests PRIVATE rynui_animation)
add_test(NAME rynui.animation_value COMMAND rynui_animation_value_tests)

add_executable(rynui_animation_easing_tests animation_easing_tests.cpp)
rynui_configure_cpp_target(rynui_animation_easing_tests)
target_link_libraries(rynui_animation_easing_tests PRIVATE rynui_animation)
add_test(NAME rynui.animation_easing COMMAND rynui_animation_easing_tests)

add_executable(rynui_animation_runtime_tests animation_runtime_tests.cpp)
rynui_configure_cpp_target(rynui_animation_runtime_tests)
target_link_libraries(rynui_animation_runtime_tests PRIVATE rynui_animation)
add_test(NAME rynui.animation_runtime COMMAND rynui_animation_runtime_tests)

add_executable(rynui_animation_deadline_tests animation_deadline_tests.cpp)
rynui_configure_cpp_target(rynui_animation_deadline_tests)
target_link_libraries(rynui_animation_deadline_tests PRIVATE rynui_animation)
add_test(NAME rynui.animation_deadline COMMAND rynui_animation_deadline_tests)

add_executable(rynui_motion_policy_tests motion_policy_tests.cpp)
rynui_configure_cpp_target(rynui_motion_policy_tests)
target_link_libraries(rynui_motion_policy_tests PRIVATE rynui_motion)
add_test(NAME rynui.motion_policy COMMAND rynui_motion_policy_tests)

add_executable(
    rynui_animation_runtime_allocation_benchmark
    animation_runtime_allocation_benchmark.cpp
)
rynui_configure_cpp_target(rynui_animation_runtime_allocation_benchmark)
target_link_libraries(
    rynui_animation_runtime_allocation_benchmark PRIVATE rynui_animation)
add_test(
    NAME rynui.animation_runtime_allocation
    COMMAND rynui_animation_runtime_allocation_benchmark
)

add_test(
    NAME rynui.animation_primitives_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_primitives_contract.cmake"
)

set(animation_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/009-20260829-build-animation-runtime-foundation/evidence"
)
add_test(
    NAME rynui.animation_generic_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${animation_evidence_directory}/platform-generic-animation-runtime.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
add_test(
    NAME rynui.animation_windows_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${animation_evidence_directory}/windows-animation-runtime.md"
        -DEXPECTED_SCOPE=windows
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
add_test(
    NAME rynui.animation_linux_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${animation_evidence_directory}/linux-animation-runtime.md"
        -DEXPECTED_SCOPE=linux
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
add_test(
    NAME rynui.animation_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${animation_evidence_directory}/windows-animation-runtime.md"
        -DEXPECTED_SCOPE=linux
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
set_tests_properties(
    rynui.animation_evidence_cross_platform_rejected
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME rynui.animation_evidence_planning_only_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/animation-evidence-planning-only.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
set_tests_properties(
    rynui.animation_evidence_planning_only_rejected
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME rynui.animation_evidence_completion_idle_required
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/animation-evidence-missing-completion.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_evidence_schema.cmake"
)
set_tests_properties(
    rynui.animation_evidence_completion_idle_required
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME rynui.animation_architecture_contract
    COMMAND ${CMAKE_COMMAND}
        "-DARCHITECTURE_FILE=${PROJECT_SOURCE_DIR}/docs/architecture.md"
        "-DREADME_FILE=${PROJECT_SOURCE_DIR}/README.md"
        "-DAGENTS_FILE=${PROJECT_SOURCE_DIR}/AGENTS.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_animation_architecture_contract.cmake"
)
add_test(
    NAME rynui.python_cache_clean
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_python_cache_clean.cmake"
)

add_executable(rynui_theme_algorithm_tests theme_algorithm_tests.cpp)
rynui_configure_cpp_target(rynui_theme_algorithm_tests)
target_link_libraries(rynui_theme_algorithm_tests PRIVATE rynui_theme)
target_compile_definitions(
    rynui_theme_algorithm_tests
    PRIVATE
        "RYNUI_THEME_GOLDEN_DIRECTORY=\"${PROJECT_SOURCE_DIR}/design-tokens/ant-design/6.5.0/golden\""
)
add_test(NAME rynui.theme_algorithm COMMAND rynui_theme_algorithm_tests)

add_executable(rynui_theme_public_api_tests theme_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_theme_public_api_tests)
target_link_libraries(rynui_theme_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.theme_public_api COMMAND rynui_theme_public_api_tests)

add_executable(rynui_theme_runtime_tests theme_runtime_tests.cpp)
rynui_configure_cpp_target(rynui_theme_runtime_tests)
target_include_directories(
    rynui_theme_runtime_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_theme_runtime_tests PRIVATE rynui_runtime rynui_theme)
add_test(NAME rynui.theme_runtime COMMAND rynui_theme_runtime_tests)

add_executable(rynui_theme_scope_component_tests theme_scope_component_tests.cpp)
rynui_configure_cpp_target(rynui_theme_scope_component_tests)
target_include_directories(
    rynui_theme_scope_component_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_theme_scope_component_tests PRIVATE rynui_components)
add_test(NAME rynui.theme_scope_component COMMAND rynui_theme_scope_component_tests)

add_executable(rynui_theme_allocation_benchmark theme_allocation_benchmark.cpp)
rynui_configure_cpp_target(rynui_theme_allocation_benchmark)
target_include_directories(
    rynui_theme_allocation_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_theme_allocation_benchmark PRIVATE rynui_runtime rynui_theme)
add_test(NAME rynui.theme_allocation COMMAND rynui_theme_allocation_benchmark)

add_executable(
    rynui_design_token_header_isolation_tests
    design_token_header_isolation_tests.cpp
)
rynui_configure_cpp_target(rynui_design_token_header_isolation_tests)
target_link_libraries(rynui_design_token_header_isolation_tests PRIVATE rynui_theme)
add_test(
    NAME rynui.design_token_header_isolation
    COMMAND rynui_design_token_header_isolation_tests
)

add_executable(rynui_prop_public_api_tests prop_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_prop_public_api_tests)
target_link_libraries(rynui_prop_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.prop_public_api COMMAND rynui_prop_public_api_tests)

add_executable(rynui_component_public_api_tests component_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_component_public_api_tests)
target_link_libraries(rynui_component_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.component_public_api COMMAND rynui_component_public_api_tests)

add_executable(rynui_layout_style_public_api_tests layout_style_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_layout_style_public_api_tests)
target_link_libraries(rynui_layout_style_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.layout_style_public_api COMMAND rynui_layout_style_public_api_tests)

add_executable(rynui_text_public_api_tests text_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_text_public_api_tests)
target_link_libraries(rynui_text_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.text_public_api COMMAND rynui_text_public_api_tests)

add_executable(rynui_button_public_api_tests button_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_button_public_api_tests)
target_link_libraries(rynui_button_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.button_public_api COMMAND rynui_button_public_api_tests)

add_executable(rynui_flex_public_api_tests flex_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_flex_public_api_tests)
target_link_libraries(rynui_flex_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.flex_public_api COMMAND rynui_flex_public_api_tests)

add_executable(rynui_space_public_api_tests space_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_space_public_api_tests)
target_link_libraries(rynui_space_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.space_public_api COMMAND rynui_space_public_api_tests)

add_executable(rynui_flex_header_isolation_tests flex_header_isolation_tests.cpp)
rynui_configure_cpp_target(rynui_flex_header_isolation_tests)
target_link_libraries(rynui_flex_header_isolation_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.flex_header_isolation COMMAND rynui_flex_header_isolation_tests)

add_executable(rynui_space_header_isolation_tests space_header_isolation_tests.cpp)
rynui_configure_cpp_target(rynui_space_header_isolation_tests)
target_link_libraries(rynui_space_header_isolation_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.space_header_isolation COMMAND rynui_space_header_isolation_tests)

add_executable(
    rynui_button_header_isolation_tests
    button_header_isolation_tests.cpp
)
rynui_configure_cpp_target(rynui_button_header_isolation_tests)
target_link_libraries(rynui_button_header_isolation_tests PRIVATE RynUI::RynUI)
add_test(
    NAME rynui.button_header_isolation
    COMMAND rynui_button_header_isolation_tests
)

add_test(
    NAME rynui.button_dependency_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_button_dependency_contract.cmake"
)

add_test(
    NAME rynui.component_theme_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_component_theme_contract.cmake"
)

add_test(
    NAME rynui.flex_dependency_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_flex_dependency_contract.cmake"
)

add_test(
    NAME rynui.space_dependency_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_space_dependency_contract.cmake"
)

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

add_executable(rynui_default_font_chain_tests default_font_chain_tests.cpp)
rynui_configure_cpp_target(rynui_default_font_chain_tests)
target_link_libraries(rynui_default_font_chain_tests PRIVATE rynui_default_fonts)
target_compile_definitions(
    rynui_default_font_chain_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.default_font_chain COMMAND rynui_default_font_chain_tests)

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

add_executable(rynui_prop_tests prop_tests.cpp)
rynui_configure_cpp_target(rynui_prop_tests)
target_include_directories(rynui_prop_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_prop_tests PRIVATE rynui_reactive)
add_test(NAME rynui.prop COMMAND rynui_prop_tests)

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

add_executable(
    rynui_component_scene_traversal_tests
    component_scene_traversal_tests.cpp
)
rynui_configure_cpp_target(rynui_component_scene_traversal_tests)
target_include_directories(
    rynui_component_scene_traversal_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(rynui_component_scene_traversal_tests PRIVATE rynui_runtime)
add_test(
    NAME rynui.component_scene_traversal
    COMMAND rynui_component_scene_traversal_tests
)

add_executable(
    rynui_component_scene_composer_tests
    component_scene_composer_tests.cpp
)
rynui_configure_cpp_target(rynui_component_scene_composer_tests)
target_link_libraries(rynui_component_scene_composer_tests PRIVATE rynui_components)
add_test(
    NAME rynui.component_scene_composer
    COMMAND rynui_component_scene_composer_tests
)

add_executable(rynui_button_scene_service_tests button_scene_service_tests.cpp)
rynui_configure_cpp_target(rynui_button_scene_service_tests)
target_link_libraries(rynui_button_scene_service_tests PRIVATE rynui_components)
add_test(NAME rynui.button_scene_service COMMAND rynui_button_scene_service_tests)

add_executable(rynui_button_component_tests button_component_tests.cpp)
rynui_configure_cpp_target(rynui_button_component_tests)
target_link_libraries(rynui_button_component_tests PRIVATE rynui_components)
target_compile_definitions(
    rynui_button_component_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.button_component COMMAND rynui_button_component_tests)

add_executable(rynui_button_spinner_benchmark button_spinner_benchmark.cpp)
rynui_configure_cpp_target(rynui_button_spinner_benchmark)
target_link_libraries(rynui_button_spinner_benchmark PRIVATE rynui_components)
add_test(NAME rynui.button_spinner_benchmark COMMAND rynui_button_spinner_benchmark)

add_executable(rynui_flex_component_tests flex_component_tests.cpp)
rynui_configure_cpp_target(rynui_flex_component_tests)
target_link_libraries(rynui_flex_component_tests PRIVATE rynui_components)
add_test(NAME rynui.flex_component COMMAND rynui_flex_component_tests)

add_executable(rynui_space_component_tests space_component_tests.cpp)
rynui_configure_cpp_target(rynui_space_component_tests)
target_link_libraries(rynui_space_component_tests PRIVATE rynui_components)
target_compile_definitions(
    rynui_space_component_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.space_component COMMAND rynui_space_component_tests)

add_executable(rynui_button_demo_frame_tests button_demo_frame_tests.cpp)
rynui_configure_cpp_target(rynui_button_demo_frame_tests)
target_link_libraries(
    rynui_button_demo_frame_tests
    PRIVATE
        rynui_components
        rynui_renderer_sdl
)
target_compile_definitions(
    rynui_button_demo_frame_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.button_demo_frame COMMAND rynui_button_demo_frame_tests)

add_executable(rynui_layout_demo_frame_tests layout_demo_frame_tests.cpp)
rynui_configure_cpp_target(rynui_layout_demo_frame_tests)
target_link_libraries(
    rynui_layout_demo_frame_tests
    PRIVATE rynui_components
)
target_compile_definitions(
    rynui_layout_demo_frame_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.layout_demo_frame COMMAND rynui_layout_demo_frame_tests)

add_executable(
    rynui_token_gallery_frame_tests
    token_gallery_frame_tests.cpp
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
)
rynui_configure_cpp_target(rynui_token_gallery_frame_tests)
target_include_directories(
    rynui_token_gallery_frame_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery"
)
target_link_libraries(
    rynui_token_gallery_frame_tests
    PRIVATE rynui_components rynui_renderer_sdl
)
target_compile_definitions(
    rynui_token_gallery_frame_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.token_gallery_frame COMMAND rynui_token_gallery_frame_tests)

add_executable(rynui_token_gallery_benchmark token_gallery_benchmark.cpp)
rynui_configure_cpp_target(rynui_token_gallery_benchmark)
target_include_directories(
    rynui_token_gallery_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_token_gallery_benchmark PRIVATE rynui_graphics rynui_theme)
add_test(NAME rynui.token_gallery_benchmark COMMAND rynui_token_gallery_benchmark)

add_test(
    NAME rynui.token_gallery_contract
    COMMAND ${CMAKE_COMMAND}
        "-DDEFINITION_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
        "-DRUNTIME_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_runtime.cpp"
        "-DEXAMPLES_CMAKE=${PROJECT_SOURCE_DIR}/examples/CMakeLists.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_contract.cmake"
)

set(token_gallery_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/006-20260827-establish-ant-design-token-system/evidence"
)
add_test(
    NAME rynui.token_gallery_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${token_gallery_evidence_directory}/linux-token-gallery-evidence.md"
        "-DWINDOWS_EVIDENCE=${token_gallery_evidence_directory}/windows-token-gallery-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_evidence_schema.cmake"
)
if(WIN32)
    add_test(
        NAME rynui.token_gallery_windows_evidence
        COMMAND ${CMAKE_COMMAND}
            "-DLINUX_EVIDENCE=${token_gallery_evidence_directory}/linux-token-gallery-evidence.md"
            "-DWINDOWS_EVIDENCE=${token_gallery_evidence_directory}/windows-token-gallery-evidence.md"
            -DREQUIRE_PASSED_PLATFORM=windows
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_evidence_schema.cmake"
    )
endif()
add_test(
    NAME rynui.token_gallery_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${token_gallery_evidence_directory}/linux-token-gallery-evidence.md"
        "-DWINDOWS_EVIDENCE=${token_gallery_evidence_directory}/linux-token-gallery-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_evidence_schema.cmake"
)
set_tests_properties(
    rynui.token_gallery_evidence_cross_platform_rejected
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME rynui.token_gallery_generic_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${token_gallery_evidence_directory}/platform-generic-token-gallery.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_generic_evidence.cmake"
)

add_test(
    NAME rynui.theme_architecture_contract
    COMMAND ${CMAKE_COMMAND}
        "-DARCHITECTURE_FILE=${PROJECT_SOURCE_DIR}/docs/architecture.md"
        "-DREADME_FILE=${PROJECT_SOURCE_DIR}/README.md"
        "-DAGENTS_FILE=${PROJECT_SOURCE_DIR}/AGENTS.md"
        "-DTOKEN_DOC_FILE=${PROJECT_SOURCE_DIR}/docs/design-tokens.md"
        "-DOPENSPEC_CONFIG_FILE=${PROJECT_SOURCE_DIR}/openspec/config.yaml"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_theme_architecture_contract.cmake"
)

add_test(
    NAME rynui.layout_example_contract
    COMMAND ${CMAKE_COMMAND}
        "-DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/examples/layout/main.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_layout_example_contract.cmake"
)

set(layout_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/005-20260827-build-flex-and-space-layout-foundation/evidence"
)
add_test(
    NAME rynui.layout_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${layout_evidence_directory}/linux-layout-evidence.md"
        "-DWINDOWS_EVIDENCE=${layout_evidence_directory}/windows-layout-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_layout_evidence_schema.cmake"
)
add_test(
    NAME rynui.layout_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${layout_evidence_directory}/linux-layout-evidence.md"
        "-DWINDOWS_EVIDENCE=${layout_evidence_directory}/linux-layout-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_layout_evidence_schema.cmake"
)
set_tests_properties(
    rynui.layout_evidence_cross_platform_rejected
    PROPERTIES WILL_FAIL TRUE
)

add_test(
    NAME rynui.button_example_contract
    COMMAND ${CMAKE_COMMAND}
        "-DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/examples/button/main.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_button_example_contract.cmake"
)

set(button_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/004-20260827-build-button-interaction-foundation/evidence"
)
add_test(
    NAME rynui.button_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${button_evidence_directory}/linux-button-evidence.md"
        "-DWINDOWS_EVIDENCE=${button_evidence_directory}/windows-button-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_button_evidence_schema.cmake"
)
add_test(
    NAME rynui.button_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DLINUX_EVIDENCE=${button_evidence_directory}/linux-button-evidence.md"
        "-DWINDOWS_EVIDENCE=${button_evidence_directory}/linux-button-evidence.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_button_evidence_schema.cmake"
)
set_tests_properties(
    rynui.button_evidence_cross_platform_rejected
    PROPERTIES WILL_FAIL TRUE
)

add_executable(rynui_layout_engine_tests layout_engine_tests.cpp)
rynui_configure_cpp_target(rynui_layout_engine_tests)
target_include_directories(rynui_layout_engine_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_layout_engine_tests PRIVATE rynui_layout)
add_test(NAME rynui.layout_engine COMMAND rynui_layout_engine_tests)

add_executable(rynui_layout_allocation_tests layout_allocation_tests.cpp)
rynui_configure_cpp_target(rynui_layout_allocation_tests)
target_include_directories(
    rynui_layout_allocation_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(rynui_layout_allocation_tests PRIVATE rynui_layout)
add_test(NAME rynui.layout_allocation COMMAND rynui_layout_allocation_tests)

add_executable(rynui_layout_style_tests layout_style_tests.cpp)
rynui_configure_cpp_target(rynui_layout_style_tests)
target_include_directories(rynui_layout_style_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_layout_style_tests
    PRIVATE rynui_layout rynui_runtime rynui_reactive
)
add_test(NAME rynui.layout_style COMMAND rynui_layout_style_tests)

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

add_executable(rynui_glyph_atlas_tests glyph_atlas_tests.cpp)
rynui_configure_cpp_target(rynui_glyph_atlas_tests)
target_include_directories(rynui_glyph_atlas_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_glyph_atlas_tests PRIVATE rynui_graphics)
target_compile_definitions(
    rynui_glyph_atlas_tests
    PRIVATE "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
)
add_test(NAME rynui.glyph_atlas COMMAND rynui_glyph_atlas_tests)

add_executable(rynui_glyph_scene_tests glyph_scene_tests.cpp)
rynui_configure_cpp_target(rynui_glyph_scene_tests)
target_include_directories(rynui_glyph_scene_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_glyph_scene_tests PRIVATE rynui_graphics rynui_text)
target_compile_definitions(
    rynui_glyph_scene_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.glyph_scene COMMAND rynui_glyph_scene_tests)

function(add_rynui_rounded_effect_test test_name source_name)
    add_executable(
        rynui_rounded_effect_${test_name}_tests
        ${source_name}
    )
    rynui_configure_cpp_target(rynui_rounded_effect_${test_name}_tests)
    target_include_directories(
        rynui_rounded_effect_${test_name}_tests
        PRIVATE "${PROJECT_SOURCE_DIR}/src"
    )
    target_link_libraries(
        rynui_rounded_effect_${test_name}_tests
        PRIVATE rynui_graphics rynui_text
    )
    add_test(
        NAME rynui.rounded_effect_${test_name}
        COMMAND rynui_rounded_effect_${test_name}_tests
    )
endfunction()

add_rynui_rounded_effect_test(math rounded_effect_math_tests.cpp)
add_rynui_rounded_effect_test(store rounded_effect_store_tests.cpp)
add_rynui_rounded_effect_test(scene rounded_effect_scene_tests.cpp)
add_rynui_rounded_effect_test(
    allocation_benchmark rounded_effect_allocation_benchmark.cpp)
add_rynui_rounded_effect_test(gpu rounded_effect_gpu_tests.cpp)

add_executable(
    rynui_rounded_effect_gpu_resources_tests
    rounded_effect_gpu_resources_tests.cpp
)
rynui_configure_cpp_target(rynui_rounded_effect_gpu_resources_tests)
target_link_libraries(
    rynui_rounded_effect_gpu_resources_tests
    PRIVATE rynui_renderer_sdl
)
add_test(
    NAME rynui.rounded_effect_gpu_resources
    COMMAND rynui_rounded_effect_gpu_resources_tests
)

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

add_executable(rynui_platform_input_tests platform_input_tests.cpp)
rynui_configure_cpp_target(rynui_platform_input_tests)
target_link_libraries(rynui_platform_input_tests PRIVATE rynui_input)
add_test(NAME rynui.platform_input COMMAND rynui_platform_input_tests)

add_executable(rynui_sdl_event_adapter_tests sdl_event_adapter_tests.cpp)
rynui_configure_cpp_target(rynui_sdl_event_adapter_tests)
target_link_libraries(
    rynui_sdl_event_adapter_tests
    PRIVATE rynui_platform_sdl SDL3::SDL3
)
add_test(NAME rynui.sdl_event_adapter COMMAND rynui_sdl_event_adapter_tests)

add_executable(rynui_platform_event_pump_tests platform_event_pump_tests.cpp)
rynui_configure_cpp_target(rynui_platform_event_pump_tests)
target_link_libraries(rynui_platform_event_pump_tests PRIVATE rynui_platform_sdl)
add_test(NAME rynui.platform_event_pump COMMAND rynui_platform_event_pump_tests)

add_test(
    NAME rynui.input_dependency_contract
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_dependency_contract.cmake"
)

add_executable(rynui_interaction_registry_tests interaction_registry_tests.cpp)
rynui_configure_cpp_target(rynui_interaction_registry_tests)
target_link_libraries(rynui_interaction_registry_tests PRIVATE rynui_interaction)
add_test(NAME rynui.interaction_registry COMMAND rynui_interaction_registry_tests)

add_executable(rynui_hit_test_tests hit_test_tests.cpp)
rynui_configure_cpp_target(rynui_hit_test_tests)
target_link_libraries(rynui_hit_test_tests PRIVATE rynui_interaction rynui_layout)
add_test(NAME rynui.hit_test COMMAND rynui_hit_test_tests)

add_executable(rynui_hit_test_refresh_tests hit_test_refresh_tests.cpp)
rynui_configure_cpp_target(rynui_hit_test_refresh_tests)
target_link_libraries(
    rynui_hit_test_refresh_tests
    PRIVATE rynui_interaction rynui_runtime
)
add_test(NAME rynui.hit_test_refresh COMMAND rynui_hit_test_refresh_tests)

add_executable(rynui_hit_test_benchmark hit_test_benchmark.cpp)
rynui_configure_cpp_target(rynui_hit_test_benchmark)
target_link_libraries(rynui_hit_test_benchmark PRIVATE rynui_interaction)
add_test(NAME rynui.hit_test_benchmark COMMAND rynui_hit_test_benchmark)

add_executable(rynui_pointer_route_tests pointer_route_tests.cpp)
rynui_configure_cpp_target(rynui_pointer_route_tests)
target_link_libraries(rynui_pointer_route_tests PRIVATE rynui_interaction)
add_test(NAME rynui.pointer_route COMMAND rynui_pointer_route_tests)

add_executable(rynui_pointer_state_tests pointer_state_tests.cpp)
rynui_configure_cpp_target(rynui_pointer_state_tests)
target_link_libraries(rynui_pointer_state_tests PRIVATE rynui_interaction)
add_test(NAME rynui.pointer_state COMMAND rynui_pointer_state_tests)

add_executable(rynui_pointer_lifecycle_tests pointer_lifecycle_tests.cpp)
rynui_configure_cpp_target(rynui_pointer_lifecycle_tests)
target_link_libraries(rynui_pointer_lifecycle_tests PRIVATE rynui_interaction)
add_test(NAME rynui.pointer_lifecycle COMMAND rynui_pointer_lifecycle_tests)

add_executable(rynui_pointer_allocation_tests pointer_allocation_tests.cpp)
rynui_configure_cpp_target(rynui_pointer_allocation_tests)
target_link_libraries(rynui_pointer_allocation_tests PRIVATE rynui_interaction)
add_test(NAME rynui.pointer_allocation COMMAND rynui_pointer_allocation_tests)

add_executable(rynui_focus_order_tests focus_order_tests.cpp)
rynui_configure_cpp_target(rynui_focus_order_tests)
target_link_libraries(rynui_focus_order_tests PRIVATE rynui_interaction)
add_test(NAME rynui.focus_order COMMAND rynui_focus_order_tests)

add_executable(rynui_focus_state_tests focus_state_tests.cpp)
rynui_configure_cpp_target(rynui_focus_state_tests)
target_link_libraries(rynui_focus_state_tests PRIVATE rynui_interaction)
add_test(NAME rynui.focus_state COMMAND rynui_focus_state_tests)

add_executable(rynui_keyboard_activation_tests keyboard_activation_tests.cpp)
rynui_configure_cpp_target(rynui_keyboard_activation_tests)
target_link_libraries(rynui_keyboard_activation_tests PRIVATE rynui_interaction)
add_test(NAME rynui.keyboard_activation COMMAND rynui_keyboard_activation_tests)

add_executable(rynui_focus_lifecycle_tests focus_lifecycle_tests.cpp)
rynui_configure_cpp_target(rynui_focus_lifecycle_tests)
target_link_libraries(rynui_focus_lifecycle_tests PRIVATE rynui_interaction)
add_test(NAME rynui.focus_lifecycle COMMAND rynui_focus_lifecycle_tests)

add_executable(rynui_focus_allocation_tests focus_allocation_tests.cpp)
rynui_configure_cpp_target(rynui_focus_allocation_tests)
target_link_libraries(rynui_focus_allocation_tests PRIVATE rynui_interaction)
add_test(NAME rynui.focus_allocation COMMAND rynui_focus_allocation_tests)

add_executable(rynui_platform_lifecycle_tests platform_lifecycle_tests.cpp)
rynui_configure_cpp_target(rynui_platform_lifecycle_tests)
target_include_directories(rynui_platform_lifecycle_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_platform_lifecycle_tests PRIVATE rynui_platform_sdl)
add_test(NAME rynui.platform_lifecycle COMMAND rynui_platform_lifecycle_tests)

add_executable(rynui_frame_renderer_tests frame_renderer_tests.cpp)
rynui_configure_cpp_target(rynui_frame_renderer_tests)
target_link_libraries(rynui_frame_renderer_tests PRIVATE rynui_renderer_sdl)
add_test(NAME rynui.frame_renderer COMMAND rynui_frame_renderer_tests)

add_executable(rynui_glyph_gpu_resources_tests glyph_gpu_resources_tests.cpp)
rynui_configure_cpp_target(rynui_glyph_gpu_resources_tests)
target_link_libraries(rynui_glyph_gpu_resources_tests PRIVATE rynui_renderer_sdl)
add_test(NAME rynui.glyph_gpu_resources COMMAND rynui_glyph_gpu_resources_tests)

add_executable(rynui_text_render_integration_tests text_render_integration_tests.cpp)
rynui_configure_cpp_target(rynui_text_render_integration_tests)
target_link_libraries(rynui_text_render_integration_tests PRIVATE rynui_renderer_sdl)
target_compile_definitions(
    rynui_text_render_integration_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_render_integration COMMAND rynui_text_render_integration_tests)

add_executable(rynui_text_scene_service_tests text_scene_service_tests.cpp)
rynui_configure_cpp_target(rynui_text_scene_service_tests)
target_link_libraries(rynui_text_scene_service_tests PRIVATE rynui_renderer_sdl)
target_compile_definitions(
    rynui_text_scene_service_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_scene_service COMMAND rynui_text_scene_service_tests)

add_executable(rynui_default_theme_tests default_theme_tests.cpp)
rynui_configure_cpp_target(rynui_default_theme_tests)
target_link_libraries(rynui_default_theme_tests PRIVATE rynui_components)
add_test(NAME rynui.default_theme COMMAND rynui_default_theme_tests)

add_executable(rynui_text_component_tests text_component_tests.cpp)
rynui_configure_cpp_target(rynui_text_component_tests)
target_link_libraries(rynui_text_component_tests PRIVATE rynui_components)
target_compile_definitions(
    rynui_text_component_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_component COMMAND rynui_text_component_tests)

add_executable(rynui_text_component_frame_tests text_component_frame_tests.cpp)
rynui_configure_cpp_target(rynui_text_component_frame_tests)
target_link_libraries(
    rynui_text_component_frame_tests
    PRIVATE
        rynui_components
        rynui_renderer_sdl
)
target_compile_definitions(
    rynui_text_component_frame_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_component_frame COMMAND rynui_text_component_frame_tests)

add_test(
    NAME rynui.text_example_contract
    COMMAND ${CMAKE_COMMAND}
        "-DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/examples/text/main.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_text_example_contract.cmake"
)

add_test(
    NAME rynui.windows_text_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE_REPORT=${PROJECT_SOURCE_DIR}/openspec/changes/002-20260826-build-text-and-cjk-rendering-foundation/evidence/windows-msvc.md"
        "-DEVIDENCE_SCREENSHOT=${PROJECT_SOURCE_DIR}/openspec/changes/002-20260826-build-text-and-cjk-rendering-foundation/evidence/windows-msvc-debug.png"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_text_evidence.cmake"
)

add_test(
    NAME rynui.linux_text_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE_REPORT=${PROJECT_SOURCE_DIR}/openspec/changes/003-20260827-build-text-component-foundation/evidence/linux-gcc.md"
        "-DEVIDENCE_SCREENSHOT=${PROJECT_SOURCE_DIR}/openspec/changes/003-20260827-build-text-component-foundation/evidence/linux-gcc-debug.png"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_linux_text_evidence.cmake"
)

add_test(
    NAME rynui.generated_shaders
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_generated_shaders.cmake"
)

add_test(
    NAME rynui.deployed_shaders
    COMMAND ${CMAKE_COMMAND}
        "-DGENERATED_SHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        "-DMINIMAL_SHADER_DIRECTORY=$<TARGET_FILE_DIR:rynui_minimal>/shaders"
        "-DTEXT_SHADER_DIRECTORY=$<TARGET_FILE_DIR:rynui_text_demo>/shaders"
        "-DBUTTON_SHADER_DIRECTORY=$<TARGET_FILE_DIR:rynui_button_demo>/shaders"
        "-DLAYOUT_SHADER_DIRECTORY=$<TARGET_FILE_DIR:rynui_layout_demo>/shaders"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_deployed_shaders.cmake"
)

add_test(
    NAME rynui.glyph_shader_contract
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_SOURCE=${PROJECT_SOURCE_DIR}/shaders/glyph.hlsl"
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_glyph_shader_contract.cmake"
)

add_test(
    NAME rynui.quad_shader_contract
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_SOURCE=${PROJECT_SOURCE_DIR}/shaders/quad.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_quad_shader_contract.cmake"
)

add_test(
    NAME rynui.rounded_effect_shader_contract
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_SOURCE=${PROJECT_SOURCE_DIR}/shaders/rounded_effect.hlsl"
        "-DSHADER_LOCK=${PROJECT_SOURCE_DIR}/shaders/rounded_effect.lock.yaml"
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_rounded_effect_shader_contract.cmake"
)

add_test(
    NAME rynui.dependency_lock
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_GIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependency_lock.cmake"
)

add_test(
    NAME rynui.prop_reference_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/prop-reference"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_prop_reference_contract.cmake"
)

add_test(
    NAME rynui.design_token_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/design-token-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_design_token_public_contract.cmake"
)

add_test(
    NAME rynui.theme_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/theme-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_theme_public_contract.cmake"
)

add_test(
    NAME rynui.text_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/text-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_text_public_contract.cmake"
)

add_test(
    NAME rynui.button_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/button-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_button_public_contract.cmake"
)

add_test(
    NAME rynui.flex_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/flex-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_flex_public_contract.cmake"
)

add_test(
    NAME rynui.space_public_compile_fail
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/space-public"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_space_public_contract.cmake"
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
