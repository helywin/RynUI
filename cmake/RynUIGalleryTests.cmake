add_executable(rynui_decoration_gallery_tests "${PROJECT_SOURCE_DIR}/tests/decoration_gallery_tests.cpp")
rynui_configure_cpp_target(rynui_decoration_gallery_tests)
target_include_directories(rynui_decoration_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_decoration_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_decoration_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(component IN ITEMS watermark affix border-beam)
    add_test(NAME rynui.decoration_gallery_${component} COMMAND rynui_decoration_gallery_tests ${component})
endforeach()

add_executable(rynui_qrcode_tour_gallery_tests "${PROJECT_SOURCE_DIR}/tests/qrcode_tour_gallery_tests.cpp")
rynui_configure_cpp_target(rynui_qrcode_tour_gallery_tests)
target_include_directories(rynui_qrcode_tour_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_qrcode_tour_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_qrcode_tour_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.qrcode_gallery COMMAND rynui_qrcode_tour_gallery_tests qrcode)

add_executable(rynui_status_gallery_tests status_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_status_gallery_tests)
target_include_directories(rynui_status_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_status_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_status_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(status_page IN ITEMS avatar badge tag)
    add_test(NAME rynui.${status_page}_native_gallery COMMAND rynui_status_gallery_tests ${status_page})
endforeach()

add_executable(rynui_carousel_gallery_tests carousel_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_carousel_gallery_tests)
target_include_directories(rynui_carousel_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_carousel_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_carousel_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.carousel_gallery COMMAND rynui_carousel_gallery_tests)


add_executable(rynui_calendar_gallery_tests calendar_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_calendar_gallery_tests)
target_include_directories(rynui_calendar_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_calendar_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_calendar_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.calendar_gallery COMMAND rynui_calendar_gallery_tests)

add_executable(rynui_mentions_gallery_tests mentions_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_mentions_gallery_tests)
target_include_directories(rynui_mentions_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_mentions_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_mentions_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.mentions_gallery COMMAND rynui_mentions_gallery_tests)

add_executable(rynui_navigation_completion_gallery_tests navigation_completion_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_navigation_completion_gallery_tests)
target_include_directories(rynui_navigation_completion_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_navigation_completion_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_navigation_completion_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.anchor_gallery COMMAND rynui_navigation_completion_gallery_tests)
add_test(NAME rynui.breadcrumb_completion_gallery COMMAND rynui_navigation_completion_gallery_tests breadcrumb)

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
add_test(NAME rynui.gallery_color_number COMMAND rynui_gallery_pages_host_tests color-number)

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
    foreach(native_sequence IN ITEMS general_layout masonry_compact shell_splitter select_autocomplete color_number mentions form tree_tree_select avatar_badge_tag)
        set(native_timeout 60)
        if(native_sequence STREQUAL "select_autocomplete")
            set(native_timeout 120)
            set(native_pages --page ant.component.auto-complete --page ant.component.select
                --page ant.component.pagination --page ant.component.tabs --page ant.component.button)
        elseif(native_sequence STREQUAL "avatar_badge_tag")
            set(native_timeout 120)
            set(native_pages --page ant.component.avatar --page ant.component.badge
                --page ant.component.tag --page ant.component.button)
        elseif(native_sequence STREQUAL "tree_tree_select")
            set(native_timeout 120)
            set(native_pages --page ant.component.tree --page ant.component.tree-select
                --page ant.component.form --page ant.component.button)
        elseif(native_sequence STREQUAL "form")
            set(native_timeout 120)
            set(native_pages --page ant.component.form --page ant.component.mentions
                --page ant.component.date-picker --page ant.component.button)
        elseif(native_sequence STREQUAL "mentions")
            set(native_timeout 120)
            set(native_pages --page ant.component.mentions --page ant.component.auto-complete
                --page ant.component.transfer --page ant.component.button)
        elseif(native_sequence STREQUAL "color_number")
            set(native_timeout 120)
            set(native_pages --page ant.component.color-picker --page ant.component.input-number --page ant.component.button)
        elseif(native_sequence STREQUAL "general_layout")
            set(native_pages --page ant.component.typography --page ant.component.grid)
        elseif(native_sequence STREQUAL "shell_splitter")
            set(native_pages --page ant.component.layout --page ant.component.splitter --page ant.component.button)
        else()
            # Preserve Masonry's third-page Dark+Compact theme without relying
            # on Typography/Grid state or on a Default-only isolated run.
            set(native_pages --page ant.component.button --page ant.component.button)
        endif()
        if(native_sequence STREQUAL "general_layout" OR native_sequence STREQUAL "masonry_compact")
            list(APPEND native_pages --page ant.component.masonry --page ant.component.button)
        endif()
        add_test(NAME rynui.gallery_native_${native_sequence}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_gallery_page_acceptance.py"
                "$<TARGET_FILE:rynui_token_gallery>" --preset "${RYNUI_CONFIGURE_PRESET}"
                --configuration "$<CONFIG>" --timeout "${native_timeout}"
                --log-dir "${PROJECT_SOURCE_DIR}/out/gallery-pages/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/${native_sequence}"
                ${native_pages})
        math(EXPR native_ctest_timeout "${native_timeout} + 25")
        set_tests_properties(rynui.gallery_native_${native_sequence} PROPERTIES
            LABELS "linux;gpu;private-wayland" TIMEOUT "${native_ctest_timeout}" RUN_SERIAL TRUE)
    endforeach()
endif()

add_executable(rynui_picker_gallery_tests picker_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_picker_gallery_tests)
target_include_directories(rynui_picker_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_picker_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_picker_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.date_picker_completion_gallery COMMAND rynui_picker_gallery_tests date)
add_test(NAME rynui.time_picker_completion_gallery COMMAND rynui_picker_gallery_tests time)

add_executable(rynui_media_gallery_tests "${PROJECT_SOURCE_DIR}/tests/media_gallery_tests.cpp")
rynui_configure_cpp_target(rynui_media_gallery_tests)
target_link_libraries(rynui_media_gallery_tests PRIVATE rynui_token_gallery_definition rynui_components)
target_compile_definitions(rynui_media_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.image_gallery COMMAND rynui_media_gallery_tests image)
add_test(NAME rynui.upload_gallery COMMAND rynui_media_gallery_tests upload)

add_executable(rynui_data_display_gallery_tests data_display_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_data_display_gallery_tests)
target_include_directories(rynui_data_display_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_data_display_gallery_tests PRIVATE rynui_token_gallery_definition rynui_token_gallery_reference_surface rynui_components)
target_compile_definitions(rynui_data_display_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(display_page IN ITEMS collapse segmented descriptions timeline)
    add_test(NAME rynui.${display_page}_gallery COMMAND rynui_data_display_gallery_tests ${display_page})
endforeach()

add_executable(rynui_gallery_visual_regression_tests "${PROJECT_SOURCE_DIR}/tests/gallery_visual_regression_tests.cpp")
rynui_configure_cpp_target(rynui_gallery_visual_regression_tests)
target_include_directories(rynui_gallery_visual_regression_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_gallery_visual_regression_tests PRIVATE
    rynui_gallery_pages_host rynui_components rynui_renderer_common)
target_compile_definitions(rynui_gallery_visual_regression_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(visual_case IN ITEMS field layout pagination tabs popup flow input)
    add_test(NAME rynui.gallery_visual_${visual_case} COMMAND rynui_gallery_visual_regression_tests ${visual_case})
endforeach()
