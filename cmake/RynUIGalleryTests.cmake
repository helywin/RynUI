add_executable(rynui_layout_splitter_gallery_tests layout_splitter_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_layout_splitter_gallery_tests)
target_include_directories(rynui_layout_splitter_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_layout_splitter_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_layout_splitter_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.shell_layout_gallery COMMAND rynui_layout_splitter_gallery_tests layout)
add_test(NAME rynui.splitter_gallery COMMAND rynui_layout_splitter_gallery_tests splitter)

# Gallery CPU contracts are available with both native and HEADLESS builds.
add_executable(rynui_grid_gallery_tests grid_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_grid_gallery_tests)
target_include_directories(rynui_grid_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_grid_gallery_tests PRIVATE rynui_token_gallery_reference_surface rynui_components)
target_compile_definitions(rynui_grid_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.grid_gallery COMMAND rynui_grid_gallery_tests)

add_executable(rynui_masonry_gallery_tests masonry_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_masonry_gallery_tests)
target_include_directories(rynui_masonry_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_masonry_gallery_tests PRIVATE rynui_token_gallery_reference_surface rynui_components)
target_compile_definitions(rynui_masonry_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.masonry_gallery COMMAND rynui_masonry_gallery_tests)

add_executable(
    rynui_token_gallery_frame_tests
    token_gallery_frame_tests.cpp
)
rynui_configure_cpp_target(rynui_token_gallery_frame_tests)
target_include_directories(
    rynui_token_gallery_frame_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery"
)
target_link_libraries(
    rynui_token_gallery_frame_tests
    PRIVATE
        rynui_token_gallery_definition
        rynui_token_gallery_reference_surface
        rynui_token_gallery_document_viewport
        rynui_components
        rynui_renderer_common
)
target_compile_definitions(
    rynui_token_gallery_frame_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.token_gallery_frame COMMAND rynui_token_gallery_frame_tests)
add_test(NAME rynui.gallery_page_input_frame COMMAND rynui_token_gallery_frame_tests --input-only)
add_test(NAME rynui.gallery_page_tree_select_frame COMMAND rynui_token_gallery_frame_tests --tree-select-only)
add_test(
    NAME rynui.gallery_platform_generic_journey
    COMMAND rynui_token_gallery_frame_tests
)

add_executable(
    rynui_ant_design_reference_catalog_tests
    ant_design_reference_catalog_tests.cpp
)
rynui_configure_cpp_target(rynui_ant_design_reference_catalog_tests)
target_link_libraries(
    rynui_ant_design_reference_catalog_tests
    PRIVATE rynui_token_gallery_definition
)
add_test(
    NAME rynui.ant_design_reference_catalog
    COMMAND rynui_ant_design_reference_catalog_tests
)

add_executable(
    rynui_gallery_document_model_tests
    gallery_document_model_tests.cpp
)

add_executable(rynui_gallery_page_model_tests gallery_page_model_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_page_model_tests)
target_link_libraries(rynui_gallery_page_model_tests PRIVATE rynui_token_gallery_definition)
add_test(NAME rynui.gallery_page_model COMMAND rynui_gallery_page_model_tests)

add_executable(rynui_gallery_draft_codec_tests gallery_draft_codec_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_draft_codec_tests)
target_include_directories(rynui_gallery_draft_codec_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_gallery_draft_codec_tests PRIVATE rynui_components)
add_test(NAME rynui.gallery_draft_codec COMMAND rynui_gallery_draft_codec_tests)

add_executable(rynui_gallery_page_controller_tests gallery_page_controller_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_page_controller_tests)
target_include_directories(rynui_gallery_page_controller_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_page_controller_tests PRIVATE rynui_token_gallery_reference_surface rynui_token_gallery_document_viewport rynui_components rynui_renderer_common)
target_compile_definitions(rynui_gallery_page_controller_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.gallery_page_controller COMMAND rynui_gallery_page_controller_tests)

add_executable(rynui_gallery_sample_page_tests gallery_sample_page_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_sample_page_tests)
target_include_directories(rynui_gallery_sample_page_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_sample_page_tests PRIVATE rynui_token_gallery_reference_surface rynui_components)
target_compile_definitions(rynui_gallery_sample_page_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.gallery_sample_page COMMAND rynui_gallery_sample_page_tests)
add_executable(rynui_gallery_source_panel_tests gallery_source_panel_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_source_panel_tests)
target_include_directories(rynui_gallery_source_panel_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_source_panel_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_gallery_source_panel_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.gallery_source_panel COMMAND rynui_gallery_source_panel_tests)
add_test(NAME rynui.gallery_sample_source_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/gallery_source_tests.py")
add_test(NAME rynui.gallery_sample_source_generated COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/generate_gallery_source.py" --root "${PROJECT_SOURCE_DIR}" --output "${rynui_gallery_source_output}" --check)

foreach(gallery_frame_case IN ITEMS select-card-tabs common data panels states forms time-picker date-picker navigation details-loading overlays-timeline members-metrics tree-autocomplete modal)
    add_test(NAME rynui.gallery_page_${gallery_frame_case}_frame COMMAND rynui_token_gallery_frame_tests --${gallery_frame_case}-only)
endforeach()

rynui_configure_cpp_target(rynui_gallery_document_model_tests)
target_link_libraries(
    rynui_gallery_document_model_tests
    PRIVATE rynui_token_gallery_definition
)
add_test(
    NAME rynui.gallery_document_model
    COMMAND rynui_gallery_document_model_tests
)

add_test(
    NAME rynui.gallery_document_contract
    COMMAND ${CMAKE_COMMAND}
        "-DDEFINITION_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
        "-DMODEL_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_document_model.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_gallery_document_contract.cmake"
)

add_executable(rynui_gallery_page_viewport_tests gallery_page_viewport_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_page_viewport_tests)
target_include_directories(rynui_gallery_page_viewport_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_page_viewport_tests PRIVATE rynui_token_gallery_document_viewport rynui_runtime)
add_test(NAME rynui.gallery_page_viewport COMMAND rynui_gallery_page_viewport_tests)

add_executable(
    rynui_gallery_document_viewport_tests
    gallery_document_viewport_tests.cpp
)
rynui_configure_cpp_target(rynui_gallery_document_viewport_tests)
target_include_directories(
    rynui_gallery_document_viewport_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_gallery_document_viewport_tests
    PRIVATE rynui_token_gallery_document_viewport rynui_runtime
)
add_test(
    NAME rynui.gallery_document_viewport
    COMMAND rynui_gallery_document_viewport_tests
)

add_test(
    NAME rynui.gallery_document_viewport_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPUBLIC_UMBRELLA=${PROJECT_SOURCE_DIR}/include/ryn/rynui.hpp"
        "-DVIEWPORT_HEADER=${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_document_viewport.hpp"
        "-DVIEWPORT_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_document_viewport.cpp"
        "-DSCROLL_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_scroll_region.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_gallery_document_viewport_contract.cmake"
)

add_executable(
    rynui_gallery_document_benchmark
    gallery_document_benchmark.cpp
)
rynui_configure_cpp_target(rynui_gallery_document_benchmark)
target_include_directories(
    rynui_gallery_document_benchmark
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_gallery_document_benchmark
    PRIVATE rynui_token_gallery_document_viewport rynui_runtime
)
add_test(
    NAME rynui.gallery_document_benchmark
    COMMAND rynui_gallery_document_benchmark
)

add_executable(rynui_token_gallery_benchmark token_gallery_benchmark.cpp)
rynui_configure_cpp_target(rynui_token_gallery_benchmark)
target_include_directories(
    rynui_token_gallery_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_token_gallery_benchmark PRIVATE rynui_graphics rynui_theme)
add_test(NAME rynui.token_gallery_benchmark COMMAND rynui_token_gallery_benchmark)


add_executable(rynui_gallery_pages_host_tests gallery_pages_host_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_pages_host_tests)
target_include_directories(rynui_gallery_pages_host_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_pages_host_tests PRIVATE rynui_gallery_pages_host rynui_components)
target_compile_definitions(rynui_gallery_pages_host_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.gallery_pages_host COMMAND rynui_gallery_pages_host_tests)

if(TARGET rynui_renderer_recording)
    add_executable(rynui_gallery_pages_benchmark gallery_pages_benchmark.cpp)
    rynui_configure_cpp_target(rynui_gallery_pages_benchmark)
    target_include_directories(rynui_gallery_pages_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(rynui_gallery_pages_benchmark PRIVATE rynui_gallery_pages_host rynui_components
        rynui_renderer_common rynui_renderer_recording)
    target_compile_definitions(rynui_gallery_pages_benchmark PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.gallery_pages_benchmark COMMAND rynui_gallery_pages_benchmark)
endif()

add_test(
    NAME rynui.ant_design_gallery_catalog_generator
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/update_ant_design_gallery_catalog.py"
        --repo-root "${PROJECT_SOURCE_DIR}"
        --check
)

add_test(
    NAME rynui.ant_design_gallery_catalog_contract
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/update_ant_design_gallery_catalog.py"
        --repo-root "${PROJECT_SOURCE_DIR}"
        --self-test
)


add_test(NAME rynui.gallery_frame_contract COMMAND rynui_token_gallery_frame_tests --frame-contract-only)

add_executable(rynui_gallery_page_acceptance_tests gallery_page_acceptance_tests.cpp)
rynui_configure_cpp_target(rynui_gallery_page_acceptance_tests)
target_link_libraries(rynui_gallery_page_acceptance_tests PRIVATE rynui_gallery_page_acceptance)
target_compile_definitions(rynui_gallery_page_acceptance_tests PRIVATE "RYNUI_GALLERY_SOURCE_ROOT=\"${PROJECT_SOURCE_DIR}\"")
add_test(NAME rynui.gallery_page_acceptance_options COMMAND rynui_gallery_page_acceptance_tests)
add_test(NAME rynui.gallery_page_acceptance_runner COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/gallery_page_acceptance_runner_tests.py" "$<TARGET_FILE:rynui_gallery_page_acceptance_tests>")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET rynui_token_gallery)
    foreach(native_sequence IN ITEMS general_layout masonry_compact)
        if(native_sequence STREQUAL "general_layout")
            set(native_pages --page ant.component.typography --page ant.component.grid)
        else()
            # Preserve Masonry's third-page Dark+Compact theme without relying
            # on Typography/Grid state or on a Default-only isolated run.
            set(native_pages --page ant.component.button --page ant.component.button)
        endif()
        add_test(NAME rynui.gallery_native_${native_sequence}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_gallery_page_acceptance.py"
                "$<TARGET_FILE:rynui_token_gallery>" --preset "${RYNUI_CONFIGURE_PRESET}"
                --configuration "$<CONFIG>" --timeout 60
                --log-dir "${PROJECT_SOURCE_DIR}/out/gallery-pages/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/${native_sequence}"
                ${native_pages} --page ant.component.masonry --page ant.component.button)
        set_tests_properties(rynui.gallery_native_${native_sequence} PROPERTIES
            LABELS "linux;gpu;private-wayland" TIMEOUT 85 RUN_SERIAL TRUE)
    endforeach()
endif()
