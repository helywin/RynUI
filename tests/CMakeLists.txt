add_executable(rynui_i18n_catalog_tests i18n_catalog_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_catalog_tests)
target_link_libraries(rynui_i18n_catalog_tests PRIVATE rynui_i18n)
add_test(NAME rynui.i18n_catalog COMMAND rynui_i18n_catalog_tests)

add_executable(rynui_i18n_context_tests i18n_context_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_context_tests)
target_link_libraries(rynui_i18n_context_tests PRIVATE rynui_components)
add_test(NAME rynui.i18n_context COMMAND rynui_i18n_context_tests)

# The application definition has only public RynUI includes; the second TU is
# the real CPU host/retained-text/editor test driver.
add_executable(rynui_i18n_public_api_tests i18n_public_api_tests.cpp
    "${PROJECT_SOURCE_DIR}/examples/i18n_app/i18n_app.cpp")
rynui_configure_cpp_target(rynui_i18n_public_api_tests)
target_link_libraries(rynui_i18n_public_api_tests PRIVATE RynUI::RynUI)
target_compile_definitions(rynui_i18n_public_api_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_public_api COMMAND rynui_i18n_public_api_tests)

add_executable(rynui_i18n_components_collections_tests i18n_components_collections_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_components_collections_tests)
target_link_libraries(rynui_i18n_components_collections_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_collections_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_collections COMMAND rynui_i18n_components_collections_tests)

foreach(i18n_components_case IN ITEMS defaults overrides)
    add_executable(rynui_i18n_components_${i18n_components_case}_tests
        i18n_components_${i18n_components_case}_tests.cpp)
    rynui_configure_cpp_target(rynui_i18n_components_${i18n_components_case}_tests)
    target_link_libraries(rynui_i18n_components_${i18n_components_case}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_i18n_components_${i18n_components_case}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.i18n_components_${i18n_components_case}
        COMMAND rynui_i18n_components_${i18n_components_case}_tests)
endforeach()

add_executable(rynui_app_service_tests app_service_tests.cpp)
rynui_configure_cpp_target(rynui_app_service_tests)
target_link_libraries(rynui_app_service_tests PRIVATE rynui_components)
target_compile_definitions(rynui_app_service_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(app_case IN ITEMS binding nested windows update callbacks failures cleanup domain clip)
    add_test(NAME rynui.app_service_${app_case} COMMAND rynui_app_service_tests ${app_case})
endforeach()

add_executable(rynui_i18n_service_tests i18n_service_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_service_tests)
target_link_libraries(rynui_i18n_service_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_service_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_service COMMAND rynui_i18n_service_tests)

add_executable(rynui_app_public_api_tests app_public_api_tests.cpp
    "${PROJECT_SOURCE_DIR}/examples/app_notice/app_notice.cpp")
rynui_configure_cpp_target(rynui_app_public_api_tests)
target_link_libraries(rynui_app_public_api_tests PRIVATE RynUI::RynUI)
target_compile_definitions(rynui_app_public_api_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.app_public_api COMMAND rynui_app_public_api_tests)

add_executable(rynui_confirm_action_tests confirm_action_tests.cpp confirm_action_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_confirm_action_tests)
target_link_libraries(rynui_confirm_action_tests PRIVATE rynui_components)
add_test(NAME rynui.confirm_action COMMAND rynui_confirm_action_tests)

add_executable(rynui_app_panel_service_tests app_panel_service_tests.cpp app_panel_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_app_panel_service_tests)
target_link_libraries(rynui_app_panel_service_tests PRIVATE rynui_components)
target_compile_definitions(rynui_app_panel_service_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(panel_case IN ITEMS lifecycle actions controlled failures windows domain retirement dispatch)
    add_test(NAME rynui.app_panel_${panel_case} COMMAND rynui_app_panel_service_tests ${panel_case})
    set_tests_properties(rynui.app_panel_${panel_case} PROPERTIES TIMEOUT 30)
endforeach()

add_executable(rynui_i18n_components_selection_tests i18n_components_selection_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_components_selection_tests)
target_link_libraries(rynui_i18n_components_selection_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_selection_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_selection COMMAND rynui_i18n_components_selection_tests)

add_executable(rynui_i18n_components_pickers_tests i18n_components_pickers_tests.cpp
    i18n_pickers_retained_cases.cpp)
rynui_configure_cpp_target(rynui_i18n_components_pickers_tests)
target_link_libraries(rynui_i18n_components_pickers_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_pickers_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_pickers COMMAND rynui_i18n_components_pickers_tests)

add_executable(rynui_i18n_components_editors_tests i18n_components_editors_tests.cpp i18n_picker_range_cases.cpp)
rynui_configure_cpp_target(rynui_i18n_components_editors_tests)
target_link_libraries(rynui_i18n_components_editors_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_editors_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_editors COMMAND rynui_i18n_components_editors_tests)

add_executable(rynui_i18n_components_media_tests i18n_components_media_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_components_media_tests)
target_link_libraries(rynui_i18n_components_media_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_media_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_media COMMAND rynui_i18n_components_media_tests)

add_executable(rynui_i18n_components_form_tests i18n_components_form_tests.cpp
    form_header_compile.cpp form_validation_header_compile.cpp)
rynui_configure_cpp_target(rynui_i18n_components_form_tests)
target_link_libraries(rynui_i18n_components_form_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_form_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_form COMMAND rynui_i18n_components_form_tests)

add_executable(rynui_i18n_components_formatting_tests i18n_components_formatting_tests.cpp)
rynui_configure_cpp_target(rynui_i18n_components_formatting_tests)
target_link_libraries(rynui_i18n_components_formatting_tests PRIVATE rynui_components)
target_compile_definitions(rynui_i18n_components_formatting_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.i18n_components_formatting COMMAND rynui_i18n_components_formatting_tests)

add_executable(rynui_overlay_descendant_maintenance_tests overlay_descendant_maintenance_tests.cpp)
rynui_configure_cpp_target(rynui_overlay_descendant_maintenance_tests)
target_link_libraries(rynui_overlay_descendant_maintenance_tests PRIVATE rynui_components)
target_compile_definitions(rynui_overlay_descendant_maintenance_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.overlay_descendant_maintenance COMMAND rynui_overlay_descendant_maintenance_tests)

add_executable(rynui_window_overlay_stack_tests window_overlay_stack_tests.cpp)
rynui_configure_cpp_target(rynui_window_overlay_stack_tests)
target_link_libraries(rynui_window_overlay_stack_tests PRIVATE rynui_components)
target_compile_definitions(rynui_window_overlay_stack_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(overlay_case IN ITEMS order domain nested focus drawer companion reentry rollback cleanup two_windows)
    add_test(NAME rynui.window_overlay_${overlay_case} COMMAND rynui_window_overlay_stack_tests ${overlay_case})
endforeach()

add_executable(rynui_popup_statistic_completion_tests popup_statistic_completion_tests.cpp)
rynui_configure_cpp_target(rynui_popup_statistic_completion_tests)
target_link_libraries(rynui_popup_statistic_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_popup_statistic_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS context scroll popup_motion locale timer statistic_motion callback)
    add_test(NAME rynui.popup_statistic_${case} COMMAND rynui_popup_statistic_completion_tests ${case})
endforeach()

if(NOT RYNUI_HAS_SDL_GPU)
    add_executable(rynui_tooltip_component_tests tooltip_component_tests.cpp)
    rynui_configure_cpp_target(rynui_tooltip_component_tests)
    target_link_libraries(rynui_tooltip_component_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_tooltip_component_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.tooltip_component COMMAND rynui_tooltip_component_tests)
endif()

add_executable(rynui_carousel_target_admission_tests carousel_target_admission_tests.cpp)
rynui_configure_cpp_target(rynui_carousel_target_admission_tests)
target_link_libraries(rynui_carousel_target_admission_tests PRIVATE rynui_components)
target_compile_definitions(rynui_carousel_target_admission_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.carousel_target_admission COMMAND rynui_carousel_target_admission_tests)

add_executable(rynui_input_constraint_tests input_constraint_tests.cpp)
rynui_configure_cpp_target(rynui_input_constraint_tests)
target_link_libraries(rynui_input_constraint_tests PRIVATE rynui_components)
target_compile_definitions(rynui_input_constraint_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS ownership algebra lifetime text callback modal hot rollback scope routes child_modal wheel participant focus_ticket)
    add_test(NAME rynui.input_constraint_${case} COMMAND rynui_input_constraint_tests ${case})
endforeach()

add_library(rynui_qrcode_public_api_compile OBJECT qrcode_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_qrcode_public_api_compile)
target_link_libraries(rynui_qrcode_public_api_compile PRIVATE rynui_components)

add_executable(rynui_qrcode_component_tests qrcode_component_tests.cpp)
rynui_configure_cpp_target(rynui_qrcode_component_tests)
target_link_libraries(rynui_qrcode_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_qrcode_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS basic geometry async errors idle theme rich form_scope paint_errors throwing_callbacks)
    add_test(NAME rynui.qrcode_component_${case} COMMAND rynui_qrcode_component_tests ${case})
endforeach()

add_executable(rynui_qrcode_model_tests qrcode_model_tests.cpp)
rynui_configure_cpp_target(rynui_qrcode_model_tests)
target_link_libraries(rynui_qrcode_model_tests PRIVATE rynui_qrcode_model)
add_test(NAME rynui.qrcode_model COMMAND rynui_qrcode_model_tests)

add_executable(rynui_retained_translation_phase_tests retained_translation_phase_tests.cpp)
rynui_configure_cpp_target(rynui_retained_translation_phase_tests)
target_link_libraries(rynui_retained_translation_phase_tests PRIVATE rynui_components)
target_compile_definitions(rynui_retained_translation_phase_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.retained_translation_phase COMMAND rynui_retained_translation_phase_tests)

add_executable(rynui_image_rounding_tests image_rounding_tests.cpp)
rynui_configure_cpp_target(rynui_image_rounding_tests)
target_link_libraries(rynui_image_rounding_tests PRIVATE rynui_image rynui_renderer_common)
add_test(NAME rynui.image_rounding COMMAND rynui_image_rounding_tests)

foreach(native_display_test IN ITEMS select_declaration image_lazy_preview form_modal_scope form_input_domain form_bound_control)
    add_executable(rynui_${native_display_test}_tests ${native_display_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${native_display_test}_tests)
    target_link_libraries(rynui_${native_display_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${native_display_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${native_display_test} COMMAND rynui_${native_display_test}_tests)
endforeach()

add_executable(rynui_tree_form_integration_tests tree_form_integration_tests.cpp)
rynui_configure_cpp_target(rynui_tree_form_integration_tests)
target_link_libraries(rynui_tree_form_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_form_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.tree_form_integration COMMAND rynui_tree_form_integration_tests)

add_executable(rynui_carousel_tests carousel_tests.cpp)
add_executable(rynui_decoration_component_tests decoration_component_tests.cpp decoration_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_decoration_component_tests)
target_link_libraries(rynui_decoration_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_decoration_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS watermark_text watermark_image watermark_async watermark_lifetime affix_top affix_bottom affix_lifetime beam theme)
    add_test(NAME rynui.decoration_${case} COMMAND rynui_decoration_component_tests ${case})
endforeach()

rynui_configure_cpp_target(rynui_carousel_tests)
target_link_libraries(rynui_carousel_tests PRIVATE rynui_components)
target_compile_definitions(rynui_carousel_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS effects controlled retained autoplay gesture failure theme interruptions pause_progress nested_lifecycle callbacks rich_controls pixels)
    add_test(NAME rynui.carousel_${case} COMMAND rynui_carousel_tests ${case})
endforeach()

add_executable(rynui_paint_opacity_tests paint_opacity_tests.cpp)
rynui_configure_cpp_target(rynui_paint_opacity_tests)
target_link_libraries(rynui_paint_opacity_tests PRIVATE rynui_components)
target_compile_definitions(rynui_paint_opacity_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.paint_opacity COMMAND rynui_paint_opacity_tests)

add_executable(rynui_paint_opacity_integration_tests paint_opacity_integration_tests.cpp)
rynui_configure_cpp_target(rynui_paint_opacity_integration_tests)
target_link_libraries(rynui_paint_opacity_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_paint_opacity_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.paint_opacity_integration COMMAND rynui_paint_opacity_integration_tests)

add_executable(rynui_retained_data_display_integration_tests retained_data_display_integration_tests.cpp)
rynui_configure_cpp_target(rynui_retained_data_display_integration_tests)
target_link_libraries(rynui_retained_data_display_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_retained_data_display_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.retained_data_display_integration COMMAND rynui_retained_data_display_integration_tests)

add_executable(rynui_calendar_tests calendar_tests.cpp)
rynui_configure_cpp_target(rynui_calendar_tests)
target_link_libraries(rynui_calendar_tests PRIVATE rynui_components)
target_compile_definitions(rynui_calendar_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS dates controlled retained failure geometry nested header keyboard theme)
    add_test(NAME rynui.calendar_${case} COMMAND rynui_calendar_tests ${case})
endforeach()

add_executable(rynui_form_integration_tests form_integration_tests.cpp)
rynui_configure_cpp_target(rynui_form_integration_tests)
target_link_libraries(rynui_form_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_form_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.form_integration COMMAND rynui_form_integration_tests)
add_library(rynui_upload_public_api_compile OBJECT upload_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_upload_public_api_compile)
target_link_libraries(rynui_upload_public_api_compile PRIVATE rynui_components)
add_executable(rynui_upload_component_tests upload_component_tests.cpp)
rynui_configure_cpp_target(rynui_upload_component_tests)
target_link_libraries(rynui_upload_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_upload_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
    "RYNUI_IMAGE_FIXTURES=\"${PROJECT_SOURCE_DIR}/tests/fixtures/images\"")
foreach(upload_case IN ITEMS basic transport controlled before dialog drop paste rich lifecycle failures theme idle form form_scope validation)
    add_test(NAME rynui.upload_component_${upload_case} COMMAND rynui_upload_component_tests ${upload_case})
endforeach()

if(RYNUI_HAS_SDL_HOST)
    add_executable(rynui_file_service_tests file_service_tests.cpp)
    rynui_configure_cpp_target(rynui_file_service_tests)
    target_link_libraries(rynui_file_service_tests PRIVATE rynui_platform_sdl SDL3::SDL3)
    target_compile_definitions(rynui_file_service_tests PRIVATE "RYNUI_IMAGE_FIXTURES=\"${PROJECT_SOURCE_DIR}/tests/fixtures/images\"")
    add_test(NAME rynui.file_service_sdk COMMAND rynui_file_service_tests)
endif()

add_executable(rynui_upload_model_tests upload_model_tests.cpp)
rynui_configure_cpp_target(rynui_upload_model_tests)
target_link_libraries(rynui_upload_model_tests PRIVATE rynui_upload_model)
foreach(upload_model_case IN ITEMS accept multiple before async_before transport controlled remove cancel reentrant validation)
    add_test(NAME rynui.upload_model_${upload_model_case} COMMAND rynui_upload_model_tests ${upload_model_case})
endforeach()

add_library(rynui_image_public_api_compile OBJECT image_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_image_public_api_compile)
target_link_libraries(rynui_image_public_api_compile PRIVATE rynui_components)

add_executable(rynui_image_component_tests image_component_tests.cpp)
rynui_configure_cpp_target(rynui_image_component_tests)
target_link_libraries(rynui_image_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_image_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
    "RYNUI_IMAGE_FIXTURES=\"${PROJECT_SOURCE_DIR}/tests/fixtures/images\"")
foreach(image_component_case IN ITEMS basic async fallback controlled group gestures failures idle model preview_source theme controlled_group rich lifetime boundaries circle ratio controller stack)
    add_test(NAME rynui.image_component_${image_component_case} COMMAND rynui_image_component_tests ${image_component_case})
endforeach()

add_executable(rynui_transfer_theme_tests transfer_theme_tests.cpp)
rynui_configure_cpp_target(rynui_transfer_theme_tests)
target_include_directories(rynui_transfer_theme_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_transfer_theme_tests PRIVATE rynui_theme)
add_test(NAME rynui.transfer_theme COMMAND rynui_transfer_theme_tests)

add_executable(rynui_transfer_complete_tests transfer_complete_tests.cpp)
rynui_configure_cpp_target(rynui_transfer_complete_tests)
target_link_libraries(rynui_transfer_complete_tests PRIVATE rynui_components)
target_compile_definitions(rynui_transfer_complete_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.transfer_complete COMMAND rynui_transfer_complete_tests)
add_executable(rynui_mentions_public_api_tests mentions_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_mentions_public_api_tests)
target_link_libraries(rynui_mentions_public_api_tests PRIVATE rynui_components)
add_test(NAME rynui.mentions_public_api COMMAND rynui_mentions_public_api_tests)

add_executable(rynui_mentions_theme_tests mentions_theme_tests.cpp)
rynui_configure_cpp_target(rynui_mentions_theme_tests)
target_link_libraries(rynui_mentions_theme_tests PRIVATE rynui_components)
add_test(NAME rynui.mentions_theme COMMAND rynui_mentions_theme_tests)


add_executable(rynui_mentions_tests mentions_tests.cpp)
rynui_configure_cpp_target(rynui_mentions_tests)
target_link_libraries(rynui_mentions_tests PRIVATE rynui_components)
target_compile_definitions(rynui_mentions_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS parsing editing caret controlled clipboard composition geometry async form window focus failures idle configuration zero nested)
    add_test(NAME rynui.mentions_${case} COMMAND rynui_mentions_tests ${case})
endforeach()

add_executable(rynui_color_picker_component_tests color_picker_component_tests.cpp)
rynui_configure_cpp_target(rynui_color_picker_component_tests)
target_link_libraries(rynui_color_picker_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_color_picker_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.color_picker_component COMMAND rynui_color_picker_component_tests)

add_executable(rynui_theme_runtime_tests theme_runtime_tests.cpp)
rynui_configure_cpp_target(rynui_theme_runtime_tests)
target_include_directories(
    rynui_theme_runtime_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_theme_runtime_tests PRIVATE rynui_runtime rynui_theme)
add_test(NAME rynui.theme_runtime COMMAND rynui_theme_runtime_tests)

foreach(menu_dropdown_test IN ITEMS menu_component dropdown_component)
    add_executable(rynui_${menu_dropdown_test}_tests ${menu_dropdown_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${menu_dropdown_test}_tests)
    target_link_libraries(rynui_${menu_dropdown_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${menu_dropdown_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${menu_dropdown_test} COMMAND rynui_${menu_dropdown_test}_tests)
endforeach()

add_executable(rynui_splitter_component_tests splitter_component_tests.cpp)
rynui_configure_cpp_target(rynui_splitter_component_tests)
target_link_libraries(rynui_splitter_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_splitter_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(splitter_case IN ITEMS model sizes drag_constraints lazy keyboard collapse_retained destroy_hidden controlled
        partial_controlled cancel_disabled nested custom_double_click callbacks motion theme_validation)
    add_test(NAME rynui.splitter_${splitter_case} COMMAND rynui_splitter_component_tests ${splitter_case})
endforeach()

add_executable(rynui_shell_layout_component_tests shell_layout_component_tests.cpp)
rynui_configure_cpp_target(rynui_shell_layout_component_tests)
target_link_libraries(rynui_shell_layout_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_shell_layout_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(shell_case IN ITEMS nested responsive shared_breakpoints controlled zero_width initial_zero theme lifecycle custom validation)
    add_test(NAME rynui.shell_layout_${shell_case} COMMAND rynui_shell_layout_component_tests ${shell_case})
endforeach()

find_package(Git REQUIRED)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
find_program(RYNUI_QRCODE_ZBAR NAMES zbarimg)
find_program(RYNUI_QRCODE_RSVG NAMES rsvg-convert)
if(RYNUI_QRCODE_ZBAR AND RYNUI_QRCODE_RSVG)
    add_test(NAME rynui.qrcode_independent_decode COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/qrcode_independent_decode.py" "$<TARGET_FILE:rynui_qrcode_model_tests>"
        "${RYNUI_QRCODE_ZBAR}" "${RYNUI_QRCODE_RSVG}" "${CMAKE_BINARY_DIR}/qrcode-exports/$<CONFIG>")
endif()

add_test(NAME rynui.qrcode_tour_theme_baseline COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/verify_qrcode_tour_theme_baseline.py")
add_test(NAME rynui.calendar_carousel_theme_baseline COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/verify_calendar_carousel_theme_baseline.py")
add_test(NAME rynui.mentions_theme_baseline COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/verify_mentions_theme_baseline.py")

add_executable(rynui_grid_component_tests grid_component_tests.cpp)
rynui_configure_cpp_target(rynui_grid_component_tests)
target_link_libraries(rynui_grid_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_grid_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(grid_case IN ITEMS breakpoints spans responsive flex alignment lifecycle)
    add_test(NAME rynui.grid_${grid_case} COMMAND rynui_grid_component_tests ${grid_case})
endforeach()

add_executable(rynui_masonry_component_tests masonry_component_tests.cpp)
rynui_configure_cpp_target(rynui_masonry_component_tests)
target_link_libraries(rynui_masonry_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_masonry_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(masonry_case IN ITEMS shortest responsive retained dynamic callbacks validation rollback lifecycle)
    add_test(NAME rynui.masonry_${masonry_case} COMMAND rynui_masonry_component_tests ${masonry_case})
endforeach()

foreach(projection_target IN ITEMS tests benchmark)
    add_executable(rynui_state_projection_${projection_target} state_projection_${projection_target}.cpp)
    rynui_configure_cpp_target(rynui_state_projection_${projection_target})
    target_link_libraries(rynui_state_projection_${projection_target} PRIVATE rynui_components)
    target_compile_definitions(rynui_state_projection_${projection_target} PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
endforeach()
foreach(projection_case IN ITEMS tree_local tree_invalidation tree_retry table_cache table_reentry table_cell_retirement table_retained table_retry)
    add_test(NAME rynui.state_projection_${projection_case} COMMAND rynui_state_projection_tests ${projection_case})
endforeach()

add_executable(rynui_tree_r2_tests tree_r2_tests.cpp)
rynui_configure_cpp_target(rynui_tree_r2_tests)
target_link_libraries(rynui_tree_r2_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_r2_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_r2 IN ITEMS nested_wheel throwing_blur_rollback slot_retirement)
    add_test(NAME rynui.tree_r2_${tree_r2} COMMAND rynui_tree_r2_tests ${tree_r2})
endforeach()

add_executable(rynui_tree_select_model_tests tree_select_model_tests.cpp)
add_executable(rynui_tree_select_component_tests tree_select_component_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_component_tests)
target_link_libraries(rynui_tree_select_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_select_case IN ITEMS basic controlled keyboard retained boundaries)
    add_test(NAME rynui.tree_select_${tree_select_case} COMMAND rynui_tree_select_component_tests ${tree_select_case})
endforeach()
rynui_configure_cpp_target(rynui_tree_select_model_tests)
target_link_libraries(rynui_tree_select_model_tests PRIVATE rynui_components)
add_test(NAME rynui.tree_select_model COMMAND rynui_tree_select_model_tests)
add_executable(rynui_tree_select_theme_tests tree_select_theme_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_theme_tests)
target_link_libraries(rynui_tree_select_theme_tests PRIVATE rynui_components)
add_test(NAME rynui.tree_select_theme COMMAND rynui_tree_select_theme_tests)
add_executable(rynui_tree_select_r3_tests tree_select_r3_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_r3_tests)
target_link_libraries(rynui_tree_select_r3_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_r3_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
foreach(tree_select_case IN ITEMS keyboard_wake cancelled_entry)
    add_test(NAME rynui.tree_select_r3_${tree_select_case} COMMAND rynui_tree_select_r3_tests ${tree_select_case})
endforeach()
add_executable(rynui_tree_select_r2_tests tree_select_r2_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_r2_tests)
target_link_libraries(rynui_tree_select_r2_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_r2_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_select_case IN ITEMS focused_outside suppressed_close)
    add_test(NAME rynui.tree_select_r2_${tree_select_case} COMMAND rynui_tree_select_r2_tests ${tree_select_case})
endforeach()
add_executable(rynui_tree_select_review_tests tree_select_review_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_review_tests)
target_link_libraries(rynui_tree_select_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_select_case IN ITEMS outside_retry disabled_entry)
    add_test(NAME rynui.tree_select_review_${tree_select_case} COMMAND rynui_tree_select_review_tests ${tree_select_case})
endforeach()
add_executable(rynui_tree_select_integration_tests tree_select_integration_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_integration_tests)
target_link_libraries(rynui_tree_select_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_select_case IN ITEMS form lifecycle rollback positioning matrix)
    add_test(NAME rynui.tree_select_${tree_select_case} COMMAND rynui_tree_select_integration_tests ${tree_select_case})
endforeach()


add_executable(rynui_tree_select_completion_tests tree_select_completion_tests.cpp)
rynui_configure_cpp_target(rynui_tree_select_completion_tests)
target_link_libraries(rynui_tree_select_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_select_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_select_completion IN ITEMS multiple dynamic_search checkbox form_reset_search responsive_idle callback_failure rich_slots)
    add_test(NAME rynui.tree_select_completion_${tree_select_completion} COMMAND rynui_tree_select_completion_tests ${tree_select_completion})
endforeach()

add_executable(rynui_tree_completion_tests tree_completion_tests.cpp)
rynui_configure_cpp_target(rynui_tree_completion_tests)
target_link_libraries(rynui_tree_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_completion IN ITEMS topology strict_range virtual async drag async_lifecycle drag_cancel async_failures virtual_drag large_checks)
    add_test(NAME rynui.tree_completion_${tree_completion} COMMAND rynui_tree_completion_tests ${tree_completion})
endforeach()

add_executable(rynui_tree_model_tests tree_model_tests.cpp)
add_executable(rynui_tree_review_tests tree_review_tests.cpp)
rynui_configure_cpp_target(rynui_tree_review_tests)
target_link_libraries(rynui_tree_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_review IN ITEMS mount_rollback zero_area translated_checkbox wheel_handoff focus_clips)
    add_test(NAME rynui.tree_review_${tree_review} COMMAND rynui_tree_review_tests ${tree_review})
endforeach()

add_executable(rynui_tree_component_tests tree_component_tests.cpp)
rynui_configure_cpp_target(rynui_tree_component_tests)
target_link_libraries(rynui_tree_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tree_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(tree_case IN ITEMS basic controlled rich scroll clips boundaries mount_lifecycle lifecycle validation reactivity theme_scene)
    add_test(NAME rynui.tree_${tree_case} COMMAND rynui_tree_component_tests ${tree_case})
endforeach()

rynui_configure_cpp_target(rynui_tree_model_tests)
target_link_libraries(rynui_tree_model_tests PRIVATE rynui_components)
add_test(NAME rynui.tree_model COMMAND rynui_tree_model_tests)

add_executable(rynui_tree_theme_tests tree_theme_tests.cpp)
rynui_configure_cpp_target(rynui_tree_theme_tests)
target_link_libraries(rynui_tree_theme_tests PRIVATE rynui_components)
add_test(NAME rynui.tree_theme COMMAND rynui_tree_theme_tests)
add_executable(rynui_component_failure_retirement_tests component_failure_retirement_tests.cpp)
rynui_configure_cpp_target(rynui_component_failure_retirement_tests)
target_link_libraries(rynui_component_failure_retirement_tests PRIVATE rynui_components)
target_compile_definitions(rynui_component_failure_retirement_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.component_failure_retirement COMMAND rynui_component_failure_retirement_tests)

foreach(auto_complete_case IN ITEMS model theme)
    add_executable(rynui_auto_complete_${auto_complete_case}_tests auto_complete_${auto_complete_case}_tests.cpp)
    rynui_configure_cpp_target(rynui_auto_complete_${auto_complete_case}_tests)
    target_link_libraries(rynui_auto_complete_${auto_complete_case}_tests PRIVATE rynui_components)
    add_test(NAME rynui.auto_complete_${auto_complete_case} COMMAND rynui_auto_complete_${auto_complete_case}_tests)
endforeach()

add_executable(rynui_auto_complete_component_tests auto_complete_component_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_component_tests)
target_link_libraries(rynui_auto_complete_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(auto_complete_case IN ITEMS flow controlled retention form example)
    add_test(NAME rynui.auto_complete_${auto_complete_case} COMMAND rynui_auto_complete_component_tests ${auto_complete_case})
endforeach()

add_executable(rynui_auto_complete_integration_tests auto_complete_integration_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_integration_tests)
target_link_libraries(rynui_auto_complete_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(auto_complete_case IN ITEMS clips children boundaries lifetime idle theme_runtime)
    add_test(NAME rynui.auto_complete_${auto_complete_case} COMMAND rynui_auto_complete_integration_tests ${auto_complete_case})
endforeach()

add_executable(rynui_auto_complete_review_tests auto_complete_review_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_review_tests)
target_link_libraries(rynui_auto_complete_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(auto_complete_case IN ITEMS mount tab zero anchor modality)
    add_test(NAME rynui.auto_complete_review_${auto_complete_case} COMMAND rynui_auto_complete_review_tests ${auto_complete_case})
endforeach()

add_executable(rynui_auto_complete_r2_tests auto_complete_r2_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_r2_tests)
target_link_libraries(rynui_auto_complete_r2_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_r2_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(auto_complete_case IN ITEMS exceptions focus)
    add_test(NAME rynui.auto_complete_r2_${auto_complete_case} COMMAND rynui_auto_complete_r2_tests ${auto_complete_case})
endforeach()

add_executable(rynui_auto_complete_r3_tests auto_complete_r3_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_r3_tests)
target_link_libraries(rynui_auto_complete_r3_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_r3_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(auto_complete_case IN ITEMS teardown blur_throw restore)
    add_test(NAME rynui.auto_complete_r3_${auto_complete_case} COMMAND rynui_auto_complete_r3_tests ${auto_complete_case})
endforeach()


add_executable(rynui_avatar_native_contract_tests avatar_native_contract_tests.cpp)
rynui_configure_cpp_target(rynui_avatar_native_contract_tests)
target_link_libraries(rynui_avatar_native_contract_tests PRIVATE rynui_components)
target_compile_definitions(rynui_avatar_native_contract_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.avatar_native_contract COMMAND rynui_avatar_native_contract_tests)

add_library(rynui_avatar_badge_tag_public_api_compile OBJECT avatar_badge_tag_public_api_compile.cpp)
rynui_configure_cpp_target(rynui_avatar_badge_tag_public_api_compile)
target_link_libraries(rynui_avatar_badge_tag_public_api_compile PRIVATE rynui_components)

foreach(status_native IN ITEMS badge tag)
    add_executable(rynui_${status_native}_native_contract_tests ${status_native}_native_contract_tests.cpp)
    rynui_configure_cpp_target(rynui_${status_native}_native_contract_tests)
    target_link_libraries(rynui_${status_native}_native_contract_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${status_native}_native_contract_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${status_native}_native_contract COMMAND rynui_${status_native}_native_contract_tests)
endforeach()

add_executable(rynui_avatar_theme_tests avatar_theme_tests.cpp)
rynui_configure_cpp_target(rynui_avatar_theme_tests)
target_link_libraries(rynui_avatar_theme_tests PRIVATE rynui_components)
add_test(NAME rynui.avatar_theme COMMAND rynui_avatar_theme_tests)

add_executable(rynui_avatar_component_tests avatar_component_tests.cpp)
rynui_configure_cpp_target(rynui_avatar_component_tests)
target_link_libraries(rynui_avatar_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_avatar_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(avatar_case IN ITEMS text group overflow clips retention gates lifecycle validation)
    add_test(NAME rynui.avatar_${avatar_case} COMMAND rynui_avatar_component_tests ${avatar_case})
endforeach()

add_executable(rynui_existing_data_entry_completion_tests existing_data_entry_completion_tests.cpp)
rynui_configure_cpp_target(rynui_existing_data_entry_completion_tests)
target_link_libraries(rynui_existing_data_entry_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_existing_data_entry_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.existing_data_entry_completion COMMAND rynui_existing_data_entry_completion_tests)

foreach(rate_test IN ITEMS rate_model rate_theme rate_public_api)
    add_executable(rynui_${rate_test}_tests ${rate_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${rate_test}_tests)
    target_link_libraries(rynui_${rate_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${rate_test} COMMAND rynui_${rate_test}_tests)
endforeach()

foreach(rate_test IN ITEMS rate_component rate_lifecycle)
    add_executable(rynui_${rate_test}_tests ${rate_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${rate_test}_tests)
    target_link_libraries(rynui_${rate_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${rate_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${rate_test} COMMAND rynui_${rate_test}_tests)
endforeach()

add_executable(rynui_rate_font_limit_tests rate_font_limit_tests.cpp)
rynui_configure_cpp_target(rynui_rate_font_limit_tests)
target_link_libraries(rynui_rate_font_limit_tests PRIVATE rynui_components)
target_compile_definitions(rynui_rate_font_limit_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.rate_font_limit COMMAND rynui_rate_font_limit_tests)

add_executable(rynui_rate_focus_order_tests rate_focus_order_tests.cpp)
rynui_configure_cpp_target(rynui_rate_focus_order_tests)
target_link_libraries(rynui_rate_focus_order_tests PRIVATE rynui_components)
target_compile_definitions(rynui_rate_focus_order_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.rate_focus_order COMMAND rynui_rate_focus_order_tests)

add_executable(rynui_rate_review_tests rate_review_tests.cpp)
rynui_configure_cpp_target(rynui_rate_review_tests)
target_link_libraries(rynui_rate_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_rate_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS geometry pointer_gap focus_exit keyboard)
    add_test(NAME rynui.rate_r1_${case} COMMAND rynui_rate_review_tests ${case})
endforeach()


add_executable(rynui_auto_complete_public_api_tests auto_complete_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_public_api_tests)
target_link_libraries(rynui_auto_complete_public_api_tests PRIVATE rynui_components)
add_test(NAME rynui.auto_complete_public_api COMMAND rynui_auto_complete_public_api_tests)

add_executable(rynui_auto_complete_complete_tests auto_complete_complete_tests.cpp)
rynui_configure_cpp_target(rynui_auto_complete_complete_tests)
target_link_libraries(rynui_auto_complete_complete_tests PRIVATE rynui_components)
target_compile_definitions(rynui_auto_complete_complete_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS window async renderers form_editor)
    add_test(NAME rynui.auto_complete_complete_${case} COMMAND rynui_auto_complete_complete_tests ${case})
endforeach()
add_executable(rynui_cascader_theme_tests cascader_theme_tests.cpp)
rynui_configure_cpp_target(rynui_cascader_theme_tests)
target_link_libraries(rynui_cascader_theme_tests PRIVATE rynui_theme)
target_include_directories(rynui_cascader_theme_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME rynui.cascader_theme COMMAND rynui_cascader_theme_tests)

if(NOT RYNUI_HAS_SDL_GPU)
    add_executable(rynui_theme_algorithm_tests theme_algorithm_tests.cpp)
    rynui_configure_cpp_target(rynui_theme_algorithm_tests)
    target_link_libraries(rynui_theme_algorithm_tests PRIVATE rynui_theme)
    target_compile_definitions(rynui_theme_algorithm_tests PRIVATE
        "RYNUI_THEME_GOLDEN_DIRECTORY=\"${PROJECT_SOURCE_DIR}/design-tokens/ant-design/6.6.5/golden\"")
    add_test(NAME rynui.theme_algorithm COMMAND rynui_theme_algorithm_tests)
endif()

add_executable(rynui_popover_review_tests popover_review_tests.cpp)
rynui_configure_cpp_target(rynui_popover_review_tests)
target_link_libraries(rynui_popover_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_popover_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS fonts positioning escape focus delays immediate)
    add_test(NAME rynui.popover_review_${case} COMMAND rynui_popover_review_tests ${case})
endforeach()

foreach(popover_test IN ITEMS popover_public_api popover_model popover_theme popover_examples)
    add_executable(rynui_${popover_test}_tests ${popover_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${popover_test}_tests)
    target_link_libraries(rynui_${popover_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${popover_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${popover_test} COMMAND rynui_${popover_test}_tests)
endforeach()

add_executable(rynui_popover_component_tests popover_component_tests.cpp)
rynui_configure_cpp_target(rynui_popover_component_tests)
target_link_libraries(rynui_popover_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_popover_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS retained triggers boundaries lifecycle validation gates)
    add_test(NAME rynui.popover_${case} COMMAND rynui_popover_component_tests ${case})
endforeach()

add_executable(rynui_popover_integration_tests popover_integration_tests.cpp)
rynui_configure_cpp_target(rynui_popover_integration_tests)
target_link_libraries(rynui_popover_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_popover_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS wheel children covered clips)
    add_test(NAME rynui.popover_${case} COMMAND rynui_popover_integration_tests ${case})
endforeach()

add_executable(rynui_notice_review_tests notice_review_tests.cpp)
rynui_configure_cpp_target(rynui_notice_review_tests)
target_link_libraries(rynui_notice_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_notice_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(notice_review_case IN ITEMS fills content_ranges clip_contract trim trim_callbacks anchor inactive_surface inactive_content)
    add_test(NAME rynui.notice_review_${notice_review_case} COMMAND rynui_notice_review_tests ${notice_review_case})
endforeach()

foreach(notice_test IN ITEMS notice_model notice_theme notice_component)
    add_executable(rynui_${notice_test}_tests ${notice_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${notice_test}_tests)
    target_link_libraries(rynui_${notice_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${notice_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    if(notice_test STREQUAL "notice_component")
        foreach(notice_case IN ITEMS event retention clock lifecycle boundaries popup clipping example)
            add_test(NAME rynui.notice_${notice_case} COMMAND rynui_notice_component_tests ${notice_case})
        endforeach()
    else()
        add_test(NAME rynui.${notice_test} COMMAND rynui_${notice_test}_tests)
    endif()
endforeach()

add_test(NAME rynui.notice_docs_example COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_notice_examples.py")

add_executable(rynui_feedback_completion_tests feedback_completion_tests.cpp)
rynui_configure_cpp_target(rynui_feedback_completion_tests)
target_link_libraries(rynui_feedback_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_feedback_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(feedback_completion_case IN ITEMS alert notice_timer notice_lifecycle notice_stack drawer drawer_push)
    add_test(NAME rynui.feedback_completion_${feedback_completion_case}
        COMMAND rynui_feedback_completion_tests ${feedback_completion_case})
endforeach()

foreach(statistic_test IN ITEMS statistic_model statistic_theme)
    add_executable(rynui_${statistic_test}_tests ${statistic_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${statistic_test}_tests)
    target_link_libraries(rynui_${statistic_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${statistic_test} COMMAND rynui_${statistic_test}_tests)
endforeach()

add_executable(rynui_statistic_component_tests statistic_component_tests.cpp)
rynui_configure_cpp_target(rynui_statistic_component_tests)
target_link_libraries(rynui_statistic_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_statistic_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(statistic_case IN ITEMS retained formatting clips lifecycle gates boundaries rollback)
    add_test(NAME rynui.statistic_${statistic_case} COMMAND rynui_statistic_component_tests ${statistic_case})
endforeach()

add_executable(rynui_statistic_public_api_tests statistic_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_statistic_public_api_tests)
target_link_libraries(rynui_statistic_public_api_tests PRIVATE rynui_components)
target_compile_definitions(rynui_statistic_public_api_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.statistic_public_api COMMAND rynui_statistic_public_api_tests)
add_test(NAME rynui.statistic_docs_example COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_statistic_examples.py")

foreach(timeline_test IN ITEMS timeline_model timeline_public_api timeline_theme)
    add_executable(rynui_${timeline_test}_tests ${timeline_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${timeline_test}_tests)
    target_link_libraries(rynui_${timeline_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${timeline_test} COMMAND rynui_${timeline_test}_tests)
endforeach()

add_executable(rynui_timeline_review_tests timeline_review_tests.cpp)
rynui_configure_cpp_target(rynui_timeline_review_tests)
target_link_libraries(rynui_timeline_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_timeline_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(timeline_case IN ITEMS fill loading capacity document_hit)
    add_test(NAME rynui.timeline_review_${timeline_case} COMMAND rynui_timeline_review_tests ${timeline_case})
endforeach()

foreach(timeline_test IN ITEMS timeline_component timeline_integration timeline_region_clip timeline_example)
    add_executable(rynui_${timeline_test}_tests ${timeline_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${timeline_test}_tests)
    target_link_libraries(rynui_${timeline_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${timeline_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${timeline_test} COMMAND rynui_${timeline_test}_tests)
endforeach()

foreach(skeleton_test IN ITEMS skeleton_public_api skeleton_model)
    add_executable(rynui_${skeleton_test}_tests ${skeleton_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${skeleton_test}_tests)
    target_link_libraries(rynui_${skeleton_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${skeleton_test} COMMAND rynui_${skeleton_test}_tests)
endforeach()

foreach(skeleton_test IN ITEMS skeleton_theme skeleton_component)
    add_executable(rynui_${skeleton_test}_tests ${skeleton_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${skeleton_test}_tests)
    target_link_libraries(rynui_${skeleton_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${skeleton_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${skeleton_test} COMMAND rynui_${skeleton_test}_tests)
endforeach()

add_executable(rynui_skeleton_integration_tests skeleton_integration_tests.cpp)
rynui_configure_cpp_target(rynui_skeleton_integration_tests)
target_link_libraries(rynui_skeleton_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_skeleton_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(skeleton_case IN ITEMS editor select retirement boundaries motion example)
    add_test(NAME rynui.skeleton_integration_${skeleton_case} COMMAND rynui_skeleton_integration_tests ${skeleton_case})
endforeach()
add_test(NAME rynui.skeleton_docs_example COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_examples.py")

add_executable(rynui_time_picker_review_tests time_picker_review_tests.cpp)
rynui_configure_cpp_target(rynui_time_picker_review_tests)
target_link_libraries(rynui_time_picker_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_time_picker_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(review_case IN ITEMS open activation reset cycles focus row_focus)
    add_test(NAME rynui.time_picker_review_${review_case} COMMAND rynui_time_picker_review_tests ${review_case})
endforeach()

add_executable(rynui_time_navigation_merge_tests time_navigation_merge_tests.cpp)
rynui_configure_cpp_target(rynui_time_navigation_merge_tests)
target_link_libraries(rynui_time_navigation_merge_tests PRIVATE rynui_components)
target_compile_definitions(rynui_time_navigation_merge_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.time_navigation_merge COMMAND rynui_time_navigation_merge_tests)


add_executable(rynui_picker_completion_tests picker_completion_tests.cpp)
rynui_configure_cpp_target(rynui_picker_completion_tests)
target_link_libraries(rynui_picker_completion_tests PRIVATE rynui_components)
target_compile_definitions(rynui_picker_completion_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(picker_case IN ITEMS format time_rules ranges date_modes native_time native_date native_ranges multiple_authority datetime_drafts failure_cleanup wheel_motion)
    add_test(NAME rynui.picker_completion_${picker_case} COMMAND rynui_picker_completion_tests ${picker_case})
endforeach()

foreach(time_test IN ITEMS time_picker_model time_picker_public_api)
    add_executable(rynui_${time_test}_tests ${time_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${time_test}_tests)
    target_link_libraries(rynui_${time_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${time_test} COMMAND rynui_${time_test}_tests)
endforeach()

foreach(time_test IN ITEMS time_picker_component time_picker_theme time_picker_integration)
    add_executable(rynui_${time_test}_tests ${time_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${time_test}_tests)
    target_link_libraries(rynui_${time_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${time_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${time_test} COMMAND rynui_${time_test}_tests)
endforeach()

foreach(date_test IN ITEMS date_picker_model date_picker_public_api)
    add_executable(rynui_${date_test}_tests ${date_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${date_test}_tests)
    target_link_libraries(rynui_${date_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${date_test} COMMAND rynui_${date_test}_tests)
endforeach()

foreach(date_test IN ITEMS date_picker_component date_picker_theme date_picker_review)
    add_executable(rynui_${date_test}_tests ${date_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${date_test}_tests)
    target_link_libraries(rynui_${date_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${date_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    if(date_test STREQUAL "date_picker_review")
        foreach(review_case IN ITEMS wheel metrics inheritance preflight form_reset)
            add_test(NAME rynui.date_picker_review_${review_case} COMMAND rynui_date_picker_review_tests ${review_case})
        endforeach()
    else()
        add_test(NAME rynui.${date_test} COMMAND rynui_${date_test}_tests)
    endif()
endforeach()

add_test(NAME rynui.form_documentation
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/form_documentation_tests.py"
        --compiler "${CMAKE_CXX_COMPILER}" --root "${PROJECT_SOURCE_DIR}"
        --output "${CMAKE_BINARY_DIR}/contract-tests/form-documentation")

foreach(form_test IN ITEMS form_model form_async form_public_api descriptions_model descriptions_public_api)
    add_executable(rynui_${form_test}_tests ${form_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${form_test}_tests)
    target_link_libraries(rynui_${form_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${form_test} COMMAND rynui_${form_test}_tests)
endforeach()

foreach(form_integration IN ITEMS form_component form_theme form_advanced form_interaction form_lifecycle descriptions_component descriptions_theme)
    add_executable(rynui_${form_integration}_tests ${form_integration}_tests.cpp)
    rynui_configure_cpp_target(rynui_${form_integration}_tests)
    target_link_libraries(rynui_${form_integration}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${form_integration}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${form_integration} COMMAND rynui_${form_integration}_tests)
endforeach()

add_executable(rynui_descriptions_integration_tests descriptions_integration_tests.cpp)
rynui_configure_cpp_target(rynui_descriptions_integration_tests)
target_link_libraries(rynui_descriptions_integration_tests PRIVATE rynui_components)
target_compile_definitions(rynui_descriptions_integration_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS popup cleanup boundary scene)
    add_test(NAME rynui.descriptions_integration_${case} COMMAND rynui_descriptions_integration_tests ${case})
endforeach()

add_executable(rynui_descriptions_region_clip_tests descriptions_region_clip_tests.cpp)
rynui_configure_cpp_target(rynui_descriptions_region_clip_tests)
target_link_libraries(rynui_descriptions_region_clip_tests PRIVATE rynui_components)
target_compile_definitions(rynui_descriptions_region_clip_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS horizontal bordered vertical vertical_bordered rich selection selection_modal)
    add_test(NAME rynui.descriptions_region_clip_${case} COMMAND rynui_descriptions_region_clip_tests ${case})
endforeach()

add_executable(rynui_descriptions_shared_theme_tests descriptions_shared_theme_tests.cpp)
rynui_configure_cpp_target(rynui_descriptions_shared_theme_tests)
target_link_libraries(rynui_descriptions_shared_theme_tests PRIVATE rynui_components)
target_compile_definitions(rynui_descriptions_shared_theme_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.descriptions_shared_theme COMMAND rynui_descriptions_shared_theme_tests)

foreach(presentation_test IN ITEMS empty_result_public_api empty_result_theme)
    add_executable(rynui_${presentation_test}_tests ${presentation_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${presentation_test}_tests)
    target_link_libraries(rynui_${presentation_test}_tests PRIVATE rynui_theme rynui_reactive rynui_string)
    target_include_directories(rynui_${presentation_test}_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME rynui.${presentation_test} COMMAND rynui_${presentation_test}_tests)
endforeach()

add_executable(rynui_empty_result_component_tests empty_result_component_tests.cpp)
rynui_configure_cpp_target(rynui_empty_result_component_tests)
target_link_libraries(rynui_empty_result_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_empty_result_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.empty_result_component COMMAND rynui_empty_result_component_tests)

add_executable(rynui_collapse_block_cleanup_tests collapse_block_cleanup_tests.cpp)
rynui_configure_cpp_target(rynui_collapse_block_cleanup_tests)
target_link_libraries(rynui_collapse_block_cleanup_tests PRIVATE rynui_components)
target_compile_definitions(rynui_collapse_block_cleanup_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.collapse_block_cleanup COMMAND rynui_collapse_block_cleanup_tests)

foreach(collapse_test IN ITEMS collapse_public_api collapse_theme collapse_component)
    add_executable(rynui_${collapse_test}_tests ${collapse_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${collapse_test}_tests)
    target_link_libraries(rynui_${collapse_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${collapse_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${collapse_test} COMMAND rynui_${collapse_test}_tests)
endforeach()

add_executable(rynui_navigation_model_tests navigation_model_tests.cpp)
rynui_configure_cpp_target(rynui_navigation_model_tests)
target_link_libraries(rynui_navigation_model_tests PRIVATE rynui_components)
add_test(NAME rynui.navigation_model COMMAND rynui_navigation_model_tests)

foreach(nav_test IN ITEMS anchor_component breadcrumb_component breadcrumb_theme steps_component steps_theme navigation_examples)
    add_executable(rynui_${nav_test}_tests ${nav_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${nav_test}_tests)
    target_link_libraries(rynui_${nav_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${nav_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${nav_test} COMMAND rynui_${nav_test}_tests)
endforeach()

add_test(NAME rynui.navigation_docs_examples
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/verify_navigation_examples.py")

add_executable(rynui_navigation_review_tests navigation_review_tests.cpp)
rynui_configure_cpp_target(rynui_navigation_review_tests)
target_link_libraries(rynui_navigation_review_tests PRIVATE rynui_components)
target_compile_definitions(rynui_navigation_review_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(review_case IN ITEMS children_steps children_breadcrumb_current children_breadcrumb_display focus_steps focus_breadcrumb)
    add_test(NAME rynui.navigation_review_${review_case} COMMAND rynui_navigation_review_tests ${review_case})
endforeach()

add_executable(rynui_navigation_block_lifetime_tests navigation_block_lifetime_tests.cpp)
rynui_configure_cpp_target(rynui_navigation_block_lifetime_tests)
target_link_libraries(rynui_navigation_block_lifetime_tests PRIVATE rynui_components)
target_compile_definitions(rynui_navigation_block_lifetime_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(widget IN ITEMS breadcrumb steps)
    foreach(lifetime_mode IN ITEMS hidden disabled inactive hidden_blur disabled_blur)
        add_test(NAME rynui.navigation_block_lifetime_${widget}_${lifetime_mode}
            COMMAND rynui_navigation_block_lifetime_tests ${widget} ${lifetime_mode})
    endforeach()
endforeach()

foreach(segmented_test IN ITEMS segmented_public_api segmented_theme segmented_component)
    add_executable(rynui_${segmented_test}_tests ${segmented_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${segmented_test}_tests)
    target_link_libraries(rynui_${segmented_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${segmented_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${segmented_test} COMMAND rynui_${segmented_test}_tests)
endforeach()

foreach(feedback_test IN ITEMS feedback_public_api feedback_theme feedback_component)
    add_executable(rynui_${feedback_test}_tests ${feedback_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${feedback_test}_tests)
    target_link_libraries(rynui_${feedback_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${feedback_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${feedback_test} COMMAND rynui_${feedback_test}_tests)
endforeach()

# Wave 2 contracts are CPU fixtures. Register independently of the historical combined feedback suite.
foreach(completion_test IN ITEMS result_completion skeleton_completion spin_completion)
    add_executable(rynui_${completion_test}_tests ${completion_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${completion_test}_tests)
    target_link_libraries(rynui_${completion_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${completion_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${completion_test} COMMAND rynui_${completion_test}_tests)
endforeach()

foreach(number_test IN ITEMS input_number_model input_number_public_api)
    add_executable(rynui_${number_test}_tests ${number_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${number_test}_tests)
    target_link_libraries(rynui_${number_test}_tests PRIVATE rynui_components)
    add_test(NAME rynui.${number_test} COMMAND rynui_${number_test}_tests)
endforeach()

foreach(number_test IN ITEMS input_number_component input_number_theme)
    add_executable(rynui_${number_test}_tests ${number_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${number_test}_tests)
    target_link_libraries(rynui_${number_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${number_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${number_test} COMMAND rynui_${number_test}_tests)
endforeach()

foreach(status_test IN ITEMS tag_badge_public_api tag_badge_theme tag_badge_component)
    add_executable(rynui_${status_test}_tests ${status_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${status_test}_tests)
    target_link_libraries(rynui_${status_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${status_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${status_test} COMMAND rynui_${status_test}_tests)
endforeach()

add_executable(rynui_bidi_analysis_tests bidi_analysis_tests.cpp)
rynui_configure_cpp_target(rynui_bidi_analysis_tests)
target_link_libraries(rynui_bidi_analysis_tests PRIVATE rynui_text)
target_compile_definitions(rynui_bidi_analysis_tests PRIVATE
    "RYNUI_BIDI_TEST_FILE=\"${RYNUI_BIDI_TEST_FILE}\""
    "RYNUI_BIDI_CHARACTER_TEST_FILE=\"${RYNUI_BIDI_CHARACTER_TEST_FILE}\"")
add_test(NAME rynui.bidi_analysis COMMAND rynui_bidi_analysis_tests)
add_executable(rynui_bidi_shaping_tests bidi_shaping_tests.cpp)
rynui_configure_cpp_target(rynui_bidi_shaping_tests)
target_link_libraries(rynui_bidi_shaping_tests PRIVATE rynui_text)
target_compile_definitions(rynui_bidi_shaping_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
    "RYNUI_VALIDATION_ARABIC_FONT=\"${RYNUI_VALIDATION_ARABIC_FONT_FILE}\""
    "RYNUI_VALIDATION_HEBREW_FONT=\"${RYNUI_VALIDATION_HEBREW_FONT_FILE}\"")
add_test(NAME rynui.bidi_shaping COMMAND rynui_bidi_shaping_tests)
add_executable(rynui_bidi_layout_tests bidi_layout_tests.cpp)
rynui_configure_cpp_target(rynui_bidi_layout_tests)
target_link_libraries(rynui_bidi_layout_tests PRIVATE rynui_text rynui_graphics)
target_compile_definitions(rynui_bidi_layout_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_ARABIC_FONT=\"${RYNUI_VALIDATION_ARABIC_FONT_FILE}\""
    "RYNUI_VALIDATION_HEBREW_FONT=\"${RYNUI_VALIDATION_HEBREW_FONT_FILE}\"")
add_test(NAME rynui.bidi_layout COMMAND rynui_bidi_layout_tests)
add_executable(rynui_bidi_caret_tests bidi_caret_tests.cpp)
rynui_configure_cpp_target(rynui_bidi_caret_tests)
target_link_libraries(rynui_bidi_caret_tests PRIVATE rynui_text)
target_compile_definitions(rynui_bidi_caret_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_ARABIC_FONT=\"${RYNUI_VALIDATION_ARABIC_FONT_FILE}\""
    "RYNUI_VALIDATION_HEBREW_FONT=\"${RYNUI_VALIDATION_HEBREW_FONT_FILE}\"")
add_test(NAME rynui.bidi_caret COMMAND rynui_bidi_caret_tests)
add_executable(rynui_bidi_input_tests bidi_input_tests.cpp)
rynui_configure_cpp_target(rynui_bidi_input_tests)
target_link_libraries(rynui_bidi_input_tests PRIVATE rynui_components)
target_compile_definitions(rynui_bidi_input_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
    "RYNUI_VALIDATION_ARABIC_FONT=\"${RYNUI_VALIDATION_ARABIC_FONT_FILE}\""
    "RYNUI_VALIDATION_HEBREW_FONT=\"${RYNUI_VALIDATION_HEBREW_FONT_FILE}\"")
add_test(NAME rynui.bidi_input COMMAND rynui_bidi_input_tests)
add_test(NAME rynui.bidi_dependency
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_bidi_dependency.py"
        --source "${PROJECT_SOURCE_DIR}" --bidi-test "${RYNUI_BIDI_TEST_FILE}"
        --character-test "${RYNUI_BIDI_CHARACTER_TEST_FILE}")
add_test(
    NAME rynui.dependency_lock
    COMMAND ${CMAKE_COMMAND}
        "-DRYNUI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_GIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependency_lock.cmake"
)

add_executable(rynui_translation_ownership_tests translation_ownership_tests.cpp
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_scroll_region.cpp")
rynui_configure_cpp_target(rynui_translation_ownership_tests)
target_include_directories(rynui_translation_ownership_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_translation_ownership_tests PRIVATE rynui_runtime)
add_test(NAME rynui.translation_ownership COMMAND rynui_translation_ownership_tests)

foreach(name IN ITEMS pagination_component table_component table_theme table_public_api table_completion tour_component)
    add_executable(rynui_${name}_tests ${name}_tests.cpp)
    rynui_configure_cpp_target(rynui_${name}_tests)
    target_link_libraries(rynui_${name}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${name}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${name} COMMAND rynui_${name}_tests)
endforeach()

foreach(glyph_replace_target IN ITEMS glyph_replace_tests glyph_replace_benchmark)
    add_executable(rynui_${glyph_replace_target} "${glyph_replace_target}.cpp")
    rynui_configure_cpp_target(rynui_${glyph_replace_target})
    target_include_directories(rynui_${glyph_replace_target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(rynui_${glyph_replace_target} PRIVATE rynui_graphics rynui_text)
endforeach()
add_test(NAME rynui.glyph_replace COMMAND rynui_glyph_replace_tests)

add_executable(rynui_reactive_fanout_benchmark reactive_fanout_benchmark.cpp)
rynui_configure_cpp_target(rynui_reactive_fanout_benchmark)
target_link_libraries(rynui_reactive_fanout_benchmark PRIVATE rynui_reactive)
add_test(NAME rynui.reactive_fanout_contract
    COMMAND rynui_reactive_fanout_benchmark --smoke --iterations 2 --warmup 1 --rounds 1)

add_executable(rynui_reactive_subscription_tests reactive_subscription_tests.cpp)
rynui_configure_cpp_target(rynui_reactive_subscription_tests)
target_link_libraries(rynui_reactive_subscription_tests PRIVATE rynui_reactive)
foreach(subscription_case IN ITEMS order read_write dynamic lifetime initial_exception flush_exception allocation)
    add_test(NAME rynui.reactive_subscription_${subscription_case}
        COMMAND rynui_reactive_subscription_tests ${subscription_case})
endforeach()

add_executable(rynui_effect_dirty_retry_tests effect_dirty_retry_tests.cpp)
rynui_configure_cpp_target(rynui_effect_dirty_retry_tests)
target_include_directories(rynui_effect_dirty_retry_tests PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(rynui_effect_dirty_retry_tests PRIVATE rynui_graphics)
foreach(effect_dirty_case IN ITEMS material geometry)
    add_test(NAME rynui.effect_dirty_retry_${effect_dirty_case}
        COMMAND rynui_effect_dirty_retry_tests ${effect_dirty_case})
endforeach()

if(TARGET rynui_renderer_recording)
add_executable(rynui_image_resource_tests image_resource_tests.cpp)
rynui_configure_cpp_target(rynui_image_resource_tests)
target_link_libraries(rynui_image_resource_tests PRIVATE rynui_image rynui_renderer_common rynui_renderer_recording)
target_compile_definitions(rynui_image_resource_tests PRIVATE "RYNUI_IMAGE_FIXTURES=\"${PROJECT_SOURCE_DIR}/tests/fixtures/images\"")
foreach(image_case IN ITEMS decode validate pixels transaction epoch capability idle)
    add_test(NAME rynui.image_resource_${image_case} COMMAND rynui_image_resource_tests ${image_case})
endforeach()

endif()

if(TARGET rynui_renderer_recording)
    add_executable(rynui_effect_range_benchmark effect_range_benchmark.cpp)
    rynui_configure_cpp_target(rynui_effect_range_benchmark)
    target_include_directories(rynui_effect_range_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(rynui_effect_range_benchmark PRIVATE rynui_renderer_common rynui_renderer_recording)
    add_test(NAME rynui.effect_range_contract
        COMMAND rynui_effect_range_benchmark --smoke --iterations 2 --warmup 1 --rounds 1)
endif()

add_executable(rynui_float_button_tests float_button_tests.cpp)
rynui_configure_cpp_target(rynui_float_button_tests)
target_link_libraries(rynui_float_button_tests PRIVATE rynui_components)
target_compile_definitions(rynui_float_button_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(float_button_case IN ITEMS basic positioning_drag anchors groups controlled_hover back_top reduced_lifecycle off_page validation)
    add_test(NAME rynui.float_button_${float_button_case} COMMAND rynui_float_button_tests ${float_button_case})
endforeach()

foreach(tabs_test IN ITEMS tabs_model tabs_theme tabs_public_api tabs_component)
    add_executable(rynui_${tabs_test}_tests ${tabs_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${tabs_test}_tests)
    target_link_libraries(rynui_${tabs_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${tabs_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${tabs_test} COMMAND rynui_${tabs_test}_tests)
endforeach()

add_executable(rynui_navigation_c_gallery_tests navigation_c_gallery_tests.cpp)
rynui_configure_cpp_target(rynui_navigation_c_gallery_tests)
target_include_directories(rynui_navigation_c_gallery_tests PRIVATE "${PROJECT_SOURCE_DIR}/examples/token_gallery")
target_link_libraries(rynui_navigation_c_gallery_tests PRIVATE rynui_token_gallery_reference_surface rynui_components)
target_compile_definitions(rynui_navigation_c_gallery_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(navigation_component IN ITEMS pagination steps tabs)
    add_test(NAME rynui.${navigation_component}_completion_gallery COMMAND rynui_navigation_c_gallery_tests ${navigation_component})
endforeach()

add_executable(rynui_select_complete_tests select_complete_tests.cpp)
rynui_configure_cpp_target(rynui_select_complete_tests)
target_link_libraries(rynui_select_complete_tests PRIVATE rynui_components)
target_compile_definitions(rynui_select_complete_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
foreach(case IN ITEMS multiple window retained async lifetime)
    add_test(NAME rynui.select_complete_${case} COMMAND rynui_select_complete_tests ${case})
endforeach()

foreach(select_test IN ITEMS select_model select_theme select_public_api select_component)
    add_executable(rynui_${select_test}_tests ${select_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${select_test}_tests)
    target_link_libraries(rynui_${select_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${select_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${select_test} COMMAND rynui_${select_test}_tests)
endforeach()

include("${PROJECT_SOURCE_DIR}/cmake/RynUIGalleryTests.cmake")

add_test(NAME rynui.data_display_docs_examples COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/verify_data_display_examples.py")

foreach(display_contract IN ITEMS collapse_contract segmented_contract descriptions_contract timeline_contract)
    add_executable(rynui_${display_contract}_tests ${display_contract}_tests.cpp)
    rynui_configure_cpp_target(rynui_${display_contract}_tests)
    target_link_libraries(rynui_${display_contract}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${display_contract}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${display_contract} COMMAND rynui_${display_contract}_tests)
endforeach()

if(NOT RYNUI_HAS_SDL_GPU)
    # Reuse affected interaction and container contracts in HEADLESS builds.
    foreach(retained_boundary_contract IN ITEMS selection_component modal_component popconfirm_component input_component)
        add_executable(rynui_${retained_boundary_contract}_tests "${retained_boundary_contract}_tests.cpp")
        rynui_configure_cpp_target(rynui_${retained_boundary_contract}_tests)
        target_link_libraries(rynui_${retained_boundary_contract}_tests PRIVATE rynui_components rynui_text_input_session)
        target_compile_definitions(rynui_${retained_boundary_contract}_tests PRIVATE
            "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
            "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
        add_test(NAME rynui.${retained_boundary_contract} COMMAND rynui_${retained_boundary_contract}_tests)
    endforeach()
    # Reuse existing dependency, batching and reentry contracts in HEADLESS builds.
    foreach(reactive_contract IN ITEMS signal memo batch scope_effect)
        add_executable(rynui_${reactive_contract}_tests "${reactive_contract}_tests.cpp")
        rynui_configure_cpp_target(rynui_${reactive_contract}_tests)
        target_link_libraries(rynui_${reactive_contract}_tests PRIVATE rynui_reactive)
        add_test(NAME rynui.${reactive_contract} COMMAND rynui_${reactive_contract}_tests)
    endforeach()
    add_subdirectory(portable)
    return()
endif()



foreach(select_test IN ITEMS card_empty_completion listy_component card_theme card_public_api card_component modal_model modal_public_api modal_theme modal_boundary modal_component drawer_model drawer_public_api drawer_theme drawer_component popconfirm_model popconfirm_public_api popconfirm_theme popconfirm_component)
    add_executable(rynui_${select_test}_tests ${select_test}_tests.cpp)
    rynui_configure_cpp_target(rynui_${select_test}_tests)
    target_link_libraries(rynui_${select_test}_tests PRIVATE rynui_components)
    target_compile_definitions(rynui_${select_test}_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.${select_test} COMMAND rynui_${select_test}_tests)
endforeach()

add_executable(rynui_otp_model_tests otp_model_tests.cpp)
rynui_configure_cpp_target(rynui_otp_model_tests)
target_link_libraries(rynui_otp_model_tests PRIVATE rynui_components)
add_test(NAME rynui.otp_model COMMAND rynui_otp_model_tests)

add_executable(rynui_otp_component_tests otp_component_tests.cpp)
rynui_configure_cpp_target(rynui_otp_component_tests)
target_link_libraries(rynui_otp_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_otp_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.otp_component COMMAND rynui_otp_component_tests)

add_executable(rynui_otp_public_api_tests otp_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_otp_public_api_tests)
target_link_libraries(rynui_otp_public_api_tests PRIVATE rynui_components)
add_test(NAME rynui.otp_public_api COMMAND rynui_otp_public_api_tests)

add_executable(rynui_otp_interaction_tests otp_interaction_tests.cpp)
rynui_configure_cpp_target(rynui_otp_interaction_tests)
target_link_libraries(rynui_otp_interaction_tests PRIVATE rynui_components)
target_compile_definitions(rynui_otp_interaction_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.otp_interaction COMMAND rynui_otp_interaction_tests)

add_executable(rynui_reactive_source_lifecycle_tests reactive_source_lifecycle_tests.cpp)
rynui_configure_cpp_target(rynui_reactive_source_lifecycle_tests)
target_link_libraries(rynui_reactive_source_lifecycle_tests PRIVATE rynui_reactive)
add_test(NAME rynui.reactive_source_lifecycle COMMAND rynui_reactive_source_lifecycle_tests)

add_executable(rynui_text_area_tests text_area_tests.cpp)
rynui_configure_cpp_target(rynui_text_area_tests)
target_link_libraries(rynui_text_area_tests PRIVATE rynui_components)
target_compile_definitions(rynui_text_area_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.text_area COMMAND rynui_text_area_tests)

add_executable(rynui_glyph_texture_upload_tests glyph_texture_upload_tests.cpp)
rynui_configure_cpp_target(rynui_glyph_texture_upload_tests)
target_link_libraries(rynui_glyph_texture_upload_tests PRIVATE rynui_renderer_common)
add_test(NAME rynui.glyph_texture_upload COMMAND rynui_glyph_texture_upload_tests)
add_executable(rynui_slider_component_tests slider_component_tests.cpp)
rynui_configure_cpp_target(rynui_slider_component_tests)
target_link_libraries(rynui_slider_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_slider_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.slider_component COMMAND rynui_slider_component_tests)

add_executable(rynui_window_overlay_tests window_overlay_tests.cpp)
rynui_configure_cpp_target(rynui_window_overlay_tests)
target_link_libraries(rynui_window_overlay_tests PRIVATE rynui_components)
add_test(NAME rynui.window_overlay COMMAND rynui_window_overlay_tests)

add_executable(rynui_tooltip_component_tests tooltip_component_tests.cpp)
rynui_configure_cpp_target(rynui_tooltip_component_tests)
target_link_libraries(rynui_tooltip_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_tooltip_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.tooltip_component COMMAND rynui_tooltip_component_tests)

add_test(
    NAME rynui.icon_asset_contract
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/verify_icon_assets.py"
)

add_executable(rynui_icon_catalog_tests icon_catalog_tests.cpp)
rynui_configure_cpp_target(rynui_icon_catalog_tests)
target_link_libraries(rynui_icon_catalog_tests PRIVATE rynui_components)
target_compile_definitions(rynui_icon_catalog_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.icon_catalog COMMAND rynui_icon_catalog_tests)

add_executable(rynui_icon_component_tests icon_component_tests.cpp)
rynui_configure_cpp_target(rynui_icon_component_tests)
target_link_libraries(rynui_icon_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_icon_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.icon_component COMMAND rynui_icon_component_tests)

add_executable(rynui_icon_vector_tests icon_vector_tests.cpp)
rynui_configure_cpp_target(rynui_icon_vector_tests)
target_link_libraries(rynui_icon_vector_tests PRIVATE rynui_components)
target_compile_definitions(rynui_icon_vector_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.icon_vector COMMAND rynui_icon_vector_tests)

add_executable(rynui_text_input_event_tests text_input_event_tests.cpp)
rynui_configure_cpp_target(rynui_text_input_event_tests)
target_link_libraries(rynui_text_input_event_tests PRIVATE rynui_input)
add_test(NAME rynui.text_input_events COMMAND rynui_text_input_event_tests)

add_executable(rynui_text_input_session_tests text_input_session_tests.cpp)
rynui_configure_cpp_target(rynui_text_input_session_tests)
target_link_libraries(rynui_text_input_session_tests PRIVATE rynui_text_input_session)
add_test(NAME rynui.text_input_session COMMAND rynui_text_input_session_tests)

add_executable(rynui_sdl_text_input_tests sdl_text_input_tests.cpp)
rynui_configure_cpp_target(rynui_sdl_text_input_tests)
target_link_libraries(rynui_sdl_text_input_tests PRIVATE rynui_text_input_session rynui_platform_sdl SDL3::SDL3)
add_test(NAME rynui.sdl_text_input COMMAND rynui_sdl_text_input_tests)

add_executable(rynui_text_input_focus_tests text_input_focus_tests.cpp)
rynui_configure_cpp_target(rynui_text_input_focus_tests)
target_link_libraries(rynui_text_input_focus_tests PRIVATE rynui_text_input_session rynui_interaction rynui_runtime)
add_test(NAME rynui.text_input_focus COMMAND rynui_text_input_focus_tests)

add_executable(rynui_text_input_atomicity_tests text_input_atomicity_tests.cpp)
rynui_configure_cpp_target(rynui_text_input_atomicity_tests)
target_link_libraries(rynui_text_input_atomicity_tests PRIVATE rynui_text_input_session rynui_platform_sdl SDL3::SDL3)
add_test(NAME rynui.text_input_atomicity COMMAND rynui_text_input_atomicity_tests)

add_executable(rynui_text_clipboard_tests text_clipboard_tests.cpp)
rynui_configure_cpp_target(rynui_text_clipboard_tests)
target_link_libraries(rynui_text_clipboard_tests PRIVATE rynui_platform_sdl SDL3::SDL3)
add_test(NAME rynui.text_clipboard COMMAND rynui_text_clipboard_tests)

add_executable(rynui_text_clipboard_command_tests text_clipboard_command_tests.cpp)
rynui_configure_cpp_target(rynui_text_clipboard_command_tests)
target_link_libraries(rynui_text_clipboard_command_tests PRIVATE rynui_text_clipboard_commands)
add_test(NAME rynui.text_clipboard_commands COMMAND rynui_text_clipboard_command_tests)

add_executable(rynui_text_history_tests text_history_tests.cpp)
rynui_configure_cpp_target(rynui_text_history_tests)
target_link_libraries(rynui_text_history_tests PRIVATE rynui_text_editor)
add_test(NAME rynui.text_history COMMAND rynui_text_history_tests)

add_executable(rynui_text_reconcile_tests text_reconcile_tests.cpp)
rynui_configure_cpp_target(rynui_text_reconcile_tests)
target_link_libraries(rynui_text_reconcile_tests PRIVATE rynui_text_editor)
add_test(NAME rynui.text_reconcile COMMAND rynui_text_reconcile_tests)

add_executable(rynui_text_editor_tests text_editor_tests.cpp)
rynui_configure_cpp_target(rynui_text_editor_tests)
target_link_libraries(rynui_text_editor_tests PRIVATE rynui_text_editor)
add_test(NAME rynui.text_editor COMMAND rynui_text_editor_tests)
add_executable(rynui_text_editor_allocation_benchmark text_editor_allocation_benchmark.cpp)
rynui_configure_cpp_target(rynui_text_editor_allocation_benchmark)
target_link_libraries(rynui_text_editor_allocation_benchmark PRIVATE rynui_text_editor)
add_test(NAME rynui.text_editor_allocation COMMAND rynui_text_editor_allocation_benchmark)

set(grapheme_source "${CMAKE_BINARY_DIR}/_deps/unicode-17/GraphemeBreakTest.txt")
rynui_download_locked_file("Unicode 17 grapheme test"
    "${RYNUI_GRAPHEME_TEST_SOURCE_URL}" "${RYNUI_GRAPHEME_TEST_SOURCE_SHA256}"
    "${grapheme_source}")
set(grapheme_fixture "${CMAKE_CURRENT_BINARY_DIR}/grapheme-fixture.txt")
set(grapheme_generator "${PROJECT_SOURCE_DIR}/tools/generate_grapheme_fixture.py")
set(grapheme_lock "${PROJECT_SOURCE_DIR}/cmake/dependencies/RynUIDependencyLock.cmake")
add_custom_command(OUTPUT "${grapheme_fixture}"
    COMMAND "${Python3_EXECUTABLE}" -B "${grapheme_generator}"
        --source "${grapheme_source}" --lock "${grapheme_lock}" --output "${grapheme_fixture}"
    DEPENDS "${grapheme_source}" "${grapheme_generator}" "${grapheme_lock}"
    VERBATIM)
add_custom_target(rynui_grapheme_fixture DEPENDS "${grapheme_fixture}")
add_executable(rynui_text_boundary_tests text_boundary_tests.cpp)
rynui_configure_cpp_target(rynui_text_boundary_tests)
target_link_libraries(rynui_text_boundary_tests PRIVATE rynui_text_boundary)
add_dependencies(rynui_text_boundary_tests rynui_grapheme_fixture)
add_test(NAME rynui.text_boundary COMMAND rynui_text_boundary_tests)
add_test(NAME rynui.unicode_grapheme_corpus
    COMMAND rynui_text_boundary_tests "${grapheme_fixture}")
add_test(NAME rynui.unicode_grapheme_fixture
    COMMAND "${Python3_EXECUTABLE}" -B "${grapheme_generator}"
        --source "${grapheme_source}" --lock "${grapheme_lock}"
        --output "${grapheme_fixture}" --check --self-test)

get_target_property(boundary_links rynui_text_boundary INTERFACE_LINK_LIBRARIES)
if(NOT boundary_links STREQUAL "$<LINK_ONLY:RynUI::Utf8proc>")
    message(FATAL_ERROR "Unicode dependency must be link-only, not a public compile dependency")
endif()
add_test(NAME rynui.unicode_dependency_contract
    COMMAND ${CMAKE_COMMAND} "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_unicode_dependency_contract.cmake")
set(utf8proc_cases positive missing-package wrong-version missing-target invalid-mode)
if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
    list(APPEND utf8proc_cases bundled-offline bundled-wrong-version)
    include(FetchContent)
    FetchContent_GetProperties(rynui_utf8proc SOURCE_DIR utf8proc_test_source)
endif()
foreach(utf8proc_case IN LISTS utf8proc_cases)
    add_test(NAME "rynui.utf8proc_resolver.${utf8proc_case}"
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/utf8proc"
            "-DTEST_CASE=${utf8proc_case}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DTEST_UTF8PROC_SOURCE=${utf8proc_test_source}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_utf8proc_resolver.cmake")
endforeach()

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

add_test(
    NAME rynui.ant_design_current_baseline
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/check_ant_design_current_baseline.py"
        --repo-root "${PROJECT_SOURCE_DIR}"
        --self-test
)

add_test(
    NAME rynui.ant_design_665_evidence_contract
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/verify_ant_design_665_evidence.py"
        --repo-root "${PROJECT_SOURCE_DIR}"
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
add_executable(rynui_material_transition_channels_tests
    material_transition_channels_tests.cpp)
rynui_configure_cpp_target(rynui_material_transition_channels_tests)
target_include_directories(rynui_material_transition_channels_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_material_transition_channels_tests PRIVATE rynui_animation)
add_test(NAME rynui.material_transition_channels
    COMMAND rynui_material_transition_channels_tests)

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
        "RYNUI_THEME_GOLDEN_DIRECTORY=\"${PROJECT_SOURCE_DIR}/design-tokens/ant-design/6.6.5/golden\""
)
add_test(NAME rynui.theme_algorithm COMMAND rynui_theme_algorithm_tests)

add_executable(rynui_theme_public_api_tests theme_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_theme_public_api_tests)
target_link_libraries(rynui_theme_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.theme_public_api COMMAND rynui_theme_public_api_tests)



add_executable(
    rynui_ant_design_button_source_contract_tests
    ant_design_button_source_contract_tests.cpp
)
rynui_configure_cpp_target(rynui_ant_design_button_source_contract_tests)
target_include_directories(
    rynui_ant_design_button_source_contract_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_ant_design_button_source_contract_tests
    PRIVATE rynui_theme
)
add_test(
    NAME rynui.ant_design_button_source_contract
    COMMAND rynui_ant_design_button_source_contract_tests
)

add_executable(rynui_ant_design_input_source_contract_tests ant_design_input_source_contract_tests.cpp)
rynui_configure_cpp_target(rynui_ant_design_input_source_contract_tests)
target_include_directories(rynui_ant_design_input_source_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_ant_design_input_source_contract_tests PRIVATE rynui_theme)
add_test(NAME rynui.ant_design_input_source_contract COMMAND rynui_ant_design_input_source_contract_tests)

add_test(NAME rynui.selection_controls_source_contract
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/verify_selection_controls_source.py")
add_test(NAME rynui.search_source_contract
    COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/verify_search_source.py")

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
add_executable(rynui_input_public_api_tests input_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_input_public_api_tests)
target_link_libraries(rynui_input_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.input_public_api COMMAND rynui_input_public_api_tests)

add_executable(rynui_search_public_api_tests search_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_search_public_api_tests)
target_link_libraries(rynui_search_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.search_public_api COMMAND rynui_search_public_api_tests)

add_executable(rynui_search_component_tests search_component_tests.cpp)
rynui_configure_cpp_target(rynui_search_component_tests)
target_include_directories(rynui_search_component_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_search_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_search_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.search_component COMMAND rynui_search_component_tests)
add_executable(rynui_search_idle_benchmark search_idle_benchmark.cpp)
rynui_configure_cpp_target(rynui_search_idle_benchmark)
target_include_directories(rynui_search_idle_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_search_idle_benchmark PRIVATE rynui_components)
target_compile_definitions(rynui_search_idle_benchmark PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.search_idle_benchmark COMMAND rynui_search_idle_benchmark)
set(search_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/014-20260925-compose-search-from-input-and-button/evidence")
add_test(NAME rynui.search_generic_evidence
    COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/verify_search_evidence.py"
        --scope platform-generic
        --evidence "${search_evidence_directory}/platform-generic-search.md")
if(WIN32)
    add_test(NAME rynui.search_windows_automated_evidence
        COMMAND "${Python3_EXECUTABLE}" -B
            "${PROJECT_SOURCE_DIR}/tools/verify_search_evidence.py"
            --scope windows-automated
            --evidence "${search_evidence_directory}/windows-search-automated.md")
    add_test(NAME rynui.search_evidence_cross_scope_rejected
        COMMAND "${Python3_EXECUTABLE}" -B
            "${PROJECT_SOURCE_DIR}/tools/verify_search_evidence.py"
            --scope platform-generic
            --evidence "${search_evidence_directory}/windows-search-automated.md")
    set_tests_properties(rynui.search_evidence_cross_scope_rejected
        PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(rynui_selection_controls_public_api_tests selection_controls_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_selection_controls_public_api_tests)
target_link_libraries(rynui_selection_controls_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.selection_controls_public_api COMMAND rynui_selection_controls_public_api_tests)

add_executable(rynui_selection_component_tests selection_component_tests.cpp)
rynui_configure_cpp_target(rynui_selection_component_tests)
target_include_directories(rynui_selection_component_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_selection_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_selection_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.selection_component COMMAND rynui_selection_component_tests)
add_executable(rynui_switch_features_tests switch_features_tests.cpp)
rynui_configure_cpp_target(rynui_switch_features_tests)
target_include_directories(rynui_switch_features_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_switch_features_tests PRIVATE rynui_components)
target_compile_definitions(rynui_switch_features_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.switch_features COMMAND rynui_switch_features_tests)
add_executable(rynui_switch_public_api_tests switch_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_switch_public_api_tests)
target_link_libraries(rynui_switch_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.switch_public_api COMMAND rynui_switch_public_api_tests)
add_executable(rynui_checkbox_group_tests checkbox_group_tests.cpp)
rynui_configure_cpp_target(rynui_checkbox_group_tests)
target_include_directories(rynui_checkbox_group_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_checkbox_group_tests PRIVATE rynui_components)
target_compile_definitions(rynui_checkbox_group_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.checkbox_group COMMAND rynui_checkbox_group_tests)
add_executable(rynui_checkbox_public_api_tests checkbox_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_checkbox_public_api_tests)
target_link_libraries(rynui_checkbox_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.checkbox_public_api COMMAND rynui_checkbox_public_api_tests)
add_executable(rynui_radio_public_api_tests radio_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_radio_public_api_tests)
target_link_libraries(rynui_radio_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.radio_public_api COMMAND rynui_radio_public_api_tests)

add_executable(rynui_radio_component_tests radio_component_tests.cpp)
rynui_configure_cpp_target(rynui_radio_component_tests)
target_include_directories(rynui_radio_component_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_radio_component_tests PRIVATE rynui_components)
target_compile_definitions(rynui_radio_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.radio_component COMMAND rynui_radio_component_tests)
add_executable(rynui_radio_features_tests radio_features_tests.cpp)
rynui_configure_cpp_target(rynui_radio_features_tests)
target_include_directories(rynui_radio_features_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_radio_features_tests PRIVATE rynui_components)
target_compile_definitions(rynui_radio_features_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.radio_features COMMAND rynui_radio_features_tests)
add_executable(rynui_selection_idle_benchmark selection_idle_benchmark.cpp)
rynui_configure_cpp_target(rynui_selection_idle_benchmark)
target_include_directories(rynui_selection_idle_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_selection_idle_benchmark PRIVATE rynui_components)
target_compile_definitions(rynui_selection_idle_benchmark PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.selection_idle_benchmark COMMAND rynui_selection_idle_benchmark)
set(selection_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/011-20260922-build-switch-and-checkbox-on-shared-control-runtime/evidence")
add_test(
    NAME rynui.selection_generic_evidence
    COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/verify_selection_controls_evidence.py"
        --scope platform-generic
        --evidence "${selection_evidence_directory}/platform-generic-selection.md")
if(WIN32)
    add_test(
        NAME rynui.selection_windows_evidence
        COMMAND "${Python3_EXECUTABLE}" -B
            "${PROJECT_SOURCE_DIR}/tools/verify_selection_controls_evidence.py"
            --scope windows
            --evidence "${selection_evidence_directory}/windows-selection.md")
    add_test(
        NAME rynui.selection_evidence_cross_scope_rejected
        COMMAND "${Python3_EXECUTABLE}" -B
            "${PROJECT_SOURCE_DIR}/tools/verify_selection_controls_evidence.py"
            --scope platform-generic
            --evidence "${selection_evidence_directory}/windows-selection.md")
    set_tests_properties(rynui.selection_evidence_cross_scope_rejected PROPERTIES WILL_FAIL TRUE)
endif()
add_test(NAME rynui.input_public_compile_fail COMMAND ${CMAKE_COMMAND}
    "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/contract-tests/input-public"
    "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
    "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DTEST_CONFIGURATION=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_public_contract.cmake")
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
add_executable(rynui_text_caret_map_tests text_caret_map_tests.cpp)
rynui_configure_cpp_target(rynui_text_caret_map_tests)
target_link_libraries(rynui_text_caret_map_tests PRIVATE rynui_text rynui_text_boundary)
target_compile_definitions(rynui_text_caret_map_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.text_caret_map COMMAND rynui_text_caret_map_tests)
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
target_link_libraries(rynui_default_font_chain_tests PRIVATE rynui_default_fonts rynui_text)
target_compile_definitions(
    rynui_default_font_chain_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.default_font_chain COMMAND rynui_default_font_chain_tests)
if(WIN32)
    add_test(NAME rynui.typography_windows_evidence_schema
        COMMAND "${Python3_EXECUTABLE}" -B
            "${PROJECT_SOURCE_DIR}/tools/verify_typography_windows_evidence.py"
            --self-test)
endif()

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
target_link_libraries(rynui_button_scene_service_tests PRIVATE rynui_components rynui_renderer_common)
add_test(NAME rynui.button_scene_service COMMAND rynui_button_scene_service_tests)

add_executable(rynui_button_component_tests button_component_tests.cpp)
add_executable(rynui_input_component_tests input_component_tests.cpp)
add_executable(rynui_input_variant_tests input_variant_tests.cpp)
rynui_configure_cpp_target(rynui_input_variant_tests)
target_link_libraries(rynui_input_variant_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_variant_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_variant COMMAND rynui_input_variant_tests)
add_executable(rynui_input_count_tests input_count_tests.cpp)
rynui_configure_cpp_target(rynui_input_count_tests)
target_link_libraries(rynui_input_count_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_count_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_count COMMAND rynui_input_count_tests)
add_executable(rynui_input_actions_tests input_actions_tests.cpp)
rynui_configure_cpp_target(rynui_input_actions_tests)
target_link_libraries(rynui_input_actions_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_actions_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_actions COMMAND rynui_input_actions_tests)
add_executable(rynui_input_display_tests input_display_tests.cpp)
rynui_configure_cpp_target(rynui_input_display_tests)
target_link_libraries(rynui_input_display_tests PRIVATE rynui_components rynui_text_editor rynui_text_boundary rynui_text)
add_test(NAME rynui.input_display COMMAND rynui_input_display_tests)
rynui_configure_cpp_target(rynui_input_component_tests)
target_link_libraries(rynui_input_component_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_component COMMAND rynui_input_component_tests)
add_executable(rynui_password_component_tests password_component_tests.cpp)
rynui_configure_cpp_target(rynui_password_component_tests)
target_link_libraries(rynui_password_component_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_password_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.password_component COMMAND rynui_password_component_tests)
add_executable(rynui_input_clear_component_tests input_clear_component_tests.cpp)
rynui_configure_cpp_target(rynui_input_clear_component_tests)
target_link_libraries(rynui_input_clear_component_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_clear_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_clear_component COMMAND rynui_input_clear_component_tests)
add_executable(rynui_input_pointer_tests input_pointer_tests.cpp)
rynui_configure_cpp_target(rynui_input_pointer_tests)
target_link_libraries(rynui_input_pointer_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_pointer_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_pointer COMMAND rynui_input_pointer_tests)
add_executable(rynui_input_keyboard_tests input_keyboard_tests.cpp)
rynui_configure_cpp_target(rynui_input_keyboard_tests)
target_link_libraries(rynui_input_keyboard_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_input_keyboard_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_keyboard COMMAND rynui_input_keyboard_tests)
add_executable(rynui_input_caret_blink_tests input_caret_blink_tests.cpp)
rynui_configure_cpp_target(rynui_input_caret_blink_tests)
target_link_libraries(rynui_input_caret_blink_tests PRIVATE rynui_components rynui_text_input_session rynui_renderer_common)
target_compile_definitions(rynui_input_caret_blink_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_caret_blink COMMAND rynui_input_caret_blink_tests)
add_executable(rynui_input_journey_tests input_journey_tests.cpp)
rynui_configure_cpp_target(rynui_input_journey_tests)
target_link_libraries(rynui_input_journey_tests PRIVATE rynui_components rynui_text_input_session rynui_renderer_common)
target_compile_definitions(rynui_input_journey_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_journey COMMAND rynui_input_journey_tests)
add_executable(rynui_input_gpu_tests input_gpu_tests.cpp)
rynui_configure_cpp_target(rynui_input_gpu_tests)
target_link_libraries(rynui_input_gpu_tests PRIVATE rynui_components rynui_text_input_session rynui_renderer_common)
target_compile_definitions(rynui_input_gpu_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_gpu COMMAND rynui_input_gpu_tests)

add_executable(rynui_input_material_transition_tests input_material_transition_tests.cpp)
rynui_configure_cpp_target(rynui_input_material_transition_tests)
target_link_libraries(rynui_input_material_transition_tests PRIVATE rynui_components)
add_test(NAME rynui.input_material_transition COMMAND rynui_input_material_transition_tests)
add_executable(rynui_input_scene_allocation_benchmark input_scene_allocation_benchmark.cpp)
rynui_configure_cpp_target(rynui_input_scene_allocation_benchmark)
target_link_libraries(rynui_input_scene_allocation_benchmark PRIVATE rynui_components rynui_text_input_session rynui_renderer_common)
target_compile_definitions(rynui_input_scene_allocation_benchmark PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.input_scene_allocation COMMAND rynui_input_scene_allocation_benchmark)

set(input_evidence_directory
    "${PROJECT_SOURCE_DIR}/openspec/changes/010-20260831-build-text-input-foundation/evidence")
add_test(
    NAME rynui.input_generic_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${input_evidence_directory}/platform-generic-input.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
add_test(
    NAME rynui.input_windows_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${input_evidence_directory}/windows-input.md"
        -DEXPECTED_SCOPE=windows
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
if(WIN32)
    add_test(
        NAME rynui.windows_input_build_evidence
        COMMAND ${CMAKE_COMMAND}
            "-DEVIDENCE=${input_evidence_directory}/windows-input-build.md"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_input_build_evidence.cmake")
    add_test(
        NAME rynui.windows_input_acceptance_runner_contract
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=${PROJECT_SOURCE_DIR}/scripts/run-windows-input-acceptance.ps1"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_input_acceptance_runner.cmake")
endif()
add_test(
    NAME rynui.input_linux_evidence_schema
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${input_evidence_directory}/linux-input.md"
        -DEXPECTED_SCOPE=linux
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
add_test(
    NAME rynui.input_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${input_evidence_directory}/windows-input.md"
        -DEXPECTED_SCOPE=linux
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
set_tests_properties(rynui.input_evidence_cross_platform_rejected PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME rynui.input_evidence_planning_only_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/input-evidence-planning-only.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
set_tests_properties(rynui.input_evidence_planning_only_rejected PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME rynui.input_evidence_commit_idle_required
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/input-evidence-missing-commit-idle.md"
        -DEXPECTED_SCOPE=platform-generic
        -DREQUIRE_PASSED=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_input_evidence_schema.cmake")
set_tests_properties(rynui.input_evidence_commit_idle_required PROPERTIES WILL_FAIL TRUE)
rynui_configure_cpp_target(rynui_button_component_tests)
target_link_libraries(rynui_button_component_tests PRIVATE rynui_components rynui_renderer_common)
target_compile_definitions(
    rynui_button_component_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.button_component COMMAND rynui_button_component_tests)

add_executable(rynui_pressable_behavior_allocation_tests pressable_behavior_allocation_tests.cpp)
rynui_configure_cpp_target(rynui_pressable_behavior_allocation_tests)
target_link_libraries(rynui_pressable_behavior_allocation_tests PRIVATE rynui_interaction)
add_test(NAME rynui.pressable_behavior_allocation COMMAND rynui_pressable_behavior_allocation_tests)

add_executable(rynui_reference_surface_tests reference_surface_tests.cpp)
rynui_configure_cpp_target(rynui_reference_surface_tests)
target_link_libraries(
    rynui_reference_surface_tests
    PRIVATE rynui_token_gallery_reference_surface rynui_components
)
target_compile_definitions(
    rynui_reference_surface_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.reference_surface COMMAND rynui_reference_surface_tests)

add_test(
    NAME rynui.reference_surface_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPUBLIC_UMBRELLA=${PROJECT_SOURCE_DIR}/include/ryn/rynui.hpp"
        "-DSURFACE_HEADER=${PROJECT_SOURCE_DIR}/examples/token_gallery/reference_surface.hpp"
        "-DSURFACE_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/reference_surface.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_reference_surface_contract.cmake"
)

add_executable(rynui_button_spinner_benchmark button_spinner_benchmark.cpp)
rynui_configure_cpp_target(rynui_button_spinner_benchmark)
target_link_libraries(rynui_button_spinner_benchmark PRIVATE rynui_components)
add_test(NAME rynui.button_spinner_benchmark COMMAND rynui_button_spinner_benchmark)

add_executable(rynui_flex_component_tests flex_component_tests.cpp)
add_executable(rynui_flex_features_tests flex_features_tests.cpp)
add_executable(rynui_space_features_tests space_features_tests.cpp)
add_executable(rynui_space_compact_tests space_compact_tests.cpp)
rynui_configure_cpp_target(rynui_space_compact_tests)
target_link_libraries(rynui_space_compact_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_space_compact_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.space_compact COMMAND rynui_space_compact_tests)
rynui_configure_cpp_target(rynui_space_features_tests)
target_link_libraries(rynui_space_features_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_space_features_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.space_features COMMAND rynui_space_features_tests)
rynui_configure_cpp_target(rynui_flex_features_tests)
target_link_libraries(rynui_flex_features_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_flex_features_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.flex_features COMMAND rynui_flex_features_tests)
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
        rynui_renderer_common
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

add_test(
    NAME rynui.token_gallery_contract
    COMMAND ${CMAKE_COMMAND}
        "-DDEFINITION_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
        "-DRUNTIME_SOURCE=${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_runtime.cpp"
        "-DEXAMPLES_CMAKE=${PROJECT_SOURCE_DIR}/examples/CMakeLists.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_token_gallery_contract.cmake"
)

set(reference_gallery_evidence
    "${PROJECT_SOURCE_DIR}/openspec/changes/008-20260829-build-ant-design-reference-gallery/evidence/platform-generic-reference-gallery.md"
)
add_test(
    NAME rynui.reference_gallery_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${reference_gallery_evidence}"
        -DEXPECTED_SCOPE=platform-generic
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_reference_gallery_evidence.cmake"
)
add_test(
    NAME rynui.reference_gallery_evidence_cross_platform_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/reference-gallery-evidence-cross-platform.md"
        -DEXPECTED_SCOPE=platform-generic
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_reference_gallery_evidence.cmake"
)
set_tests_properties(
    rynui.reference_gallery_evidence_cross_platform_rejected
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME rynui.reference_gallery_evidence_planning_only_rejected
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/reference-gallery-evidence-planning-only.md"
        -DEXPECTED_SCOPE=platform-generic
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_reference_gallery_evidence.cmake"
)
set_tests_properties(
    rynui.reference_gallery_evidence_planning_only_rejected
    PROPERTIES WILL_FAIL TRUE
)
if(WIN32)
    add_test(
        NAME rynui.windows_reference_gallery_build_evidence
        COMMAND ${CMAKE_COMMAND}
            "-DEVIDENCE=${PROJECT_SOURCE_DIR}/openspec/changes/008-20260829-build-ant-design-reference-gallery/evidence/windows-reference-gallery-build.md"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_reference_gallery_build_evidence.cmake"
    )
    add_test(
        NAME rynui.windows_gallery_acceptance_runner_contract
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=${PROJECT_SOURCE_DIR}/scripts/run-windows-gallery-acceptance.ps1"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_gallery_acceptance_runner.cmake"
    )
endif()

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
add_executable(rynui_logical_scene_packing_tests logical_scene_packing_tests.cpp)
rynui_configure_cpp_target(rynui_logical_scene_packing_tests)
target_link_libraries(rynui_logical_scene_packing_tests PRIVATE rynui_renderer_common)
add_test(NAME rynui.logical_scene_packing COMMAND rynui_logical_scene_packing_tests)
rynui_configure_cpp_target(rynui_quad_primitive_tests)
target_include_directories(rynui_quad_primitive_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_quad_primitive_tests PRIVATE rynui_renderer_common)
add_test(NAME rynui.quad_primitive COMMAND rynui_quad_primitive_tests)

add_executable(rynui_buffer_upload_batch_layout_tests buffer_upload_batch_layout_tests.cpp)
rynui_configure_cpp_target(rynui_buffer_upload_batch_layout_tests)
target_include_directories(rynui_buffer_upload_batch_layout_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME rynui.buffer_upload_batch_layout
    COMMAND rynui_buffer_upload_batch_layout_tests)

add_executable(rynui_texture_upload_batch_layout_tests
    texture_upload_batch_layout_tests.cpp)
rynui_configure_cpp_target(rynui_texture_upload_batch_layout_tests)
target_include_directories(rynui_texture_upload_batch_layout_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME rynui.texture_upload_batch_layout
    COMMAND rynui_texture_upload_batch_layout_tests)

add_executable(rynui_rendering_locality_benchmark rendering_locality_benchmark.cpp)
rynui_configure_cpp_target(rynui_rendering_locality_benchmark)
target_include_directories(rynui_rendering_locality_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_rendering_locality_benchmark PRIVATE rynui_graphics rynui_runtime)
add_test(NAME rynui.rendering_locality COMMAND rynui_rendering_locality_benchmark)

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
target_link_libraries(rynui_glyph_scene_tests PRIVATE rynui_renderer_common rynui_text)
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
add_rynui_rounded_effect_test(paint effect_paint_tests.cpp)
add_rynui_rounded_effect_test(cap arc_cap_tests.cpp)
add_rynui_rounded_effect_test(store rounded_effect_store_tests.cpp)
add_rynui_rounded_effect_test(scene rounded_effect_scene_tests.cpp)
add_rynui_rounded_effect_test(
    allocation_benchmark rounded_effect_allocation_benchmark.cpp)
add_rynui_rounded_effect_test(gpu rounded_effect_gpu_tests.cpp)
target_link_libraries(rynui_rounded_effect_gpu_tests PRIVATE rynui_renderer_common)

add_executable(rynui_effect_paint_packing_tests effect_paint_packing_tests.cpp)
rynui_configure_cpp_target(rynui_effect_paint_packing_tests)
target_link_libraries(rynui_effect_paint_packing_tests PRIVATE rynui_renderer_common)
add_test(NAME rynui.effect_paint_packing COMMAND rynui_effect_paint_packing_tests)

add_executable(
    rynui_rounded_effect_gpu_resources_tests
    rounded_effect_gpu_resources_tests.cpp
)
rynui_configure_cpp_target(rynui_rounded_effect_gpu_resources_tests)
target_link_libraries(
    rynui_rounded_effect_gpu_resources_tests
    PRIVATE rynui_renderer_common
)
add_test(
    NAME rynui.rounded_effect_gpu_resources
    COMMAND rynui_rounded_effect_gpu_resources_tests
)

if(TARGET rynui_renderer_recording)
    add_executable(rynui_effect_preflight_tests effect_preflight_tests.cpp)
    rynui_configure_cpp_target(rynui_effect_preflight_tests)
    target_link_libraries(rynui_effect_preflight_tests PRIVATE rynui_renderer_common rynui_renderer_recording)
    add_test(NAME rynui.effect_preflight COMMAND rynui_effect_preflight_tests)

    add_executable(rynui_effect_paint_resources_tests effect_paint_resources_tests.cpp)
    rynui_configure_cpp_target(rynui_effect_paint_resources_tests)
    target_link_libraries(rynui_effect_paint_resources_tests PRIVATE rynui_renderer_common rynui_renderer_recording)
    add_test(NAME rynui.effect_paint_resources COMMAND rynui_effect_paint_resources_tests)

    add_executable(rynui_cap_geometry_tests cap_geometry_tests.cpp)
    rynui_configure_cpp_target(rynui_cap_geometry_tests)
    target_link_libraries(rynui_cap_geometry_tests PRIVATE rynui_renderer_common rynui_renderer_recording)
    add_test(NAME rynui.cap_geometry COMMAND rynui_cap_geometry_tests)

    add_executable(rynui_effect_compose_guard_tests effect_compose_guard_tests.cpp)
    rynui_configure_cpp_target(rynui_effect_compose_guard_tests)
    target_link_libraries(rynui_effect_compose_guard_tests PRIVATE
        rynui_components rynui_renderer_common rynui_renderer_recording)
    target_compile_definitions(rynui_effect_compose_guard_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\"")
    add_test(NAME rynui.effect_compose_guard COMMAND rynui_effect_compose_guard_tests)
endif()

add_executable(rynui_binding_quad_integration_tests binding_quad_integration_tests.cpp)
rynui_configure_cpp_target(rynui_binding_quad_integration_tests)
target_include_directories(rynui_binding_quad_integration_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    rynui_binding_quad_integration_tests
    PRIVATE rynui_renderer_common rynui_reactive
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

add_executable(rynui_hit_test_dirty_refresh_benchmark hit_test_dirty_refresh_benchmark.cpp)
rynui_configure_cpp_target(rynui_hit_test_dirty_refresh_benchmark)
target_link_libraries(rynui_hit_test_dirty_refresh_benchmark PRIVATE rynui_interaction)
add_test(NAME rynui.hit_test_dirty_refresh_benchmark
    COMMAND rynui_hit_test_dirty_refresh_benchmark)

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
target_link_libraries(rynui_platform_lifecycle_tests PRIVATE rynui_platform_sdl rynui_renderer_sdl)
add_test(NAME rynui.platform_lifecycle COMMAND rynui_platform_lifecycle_tests)

add_executable(rynui_frame_renderer_tests frame_renderer_tests.cpp)
rynui_configure_cpp_target(rynui_frame_renderer_tests)
target_link_libraries(rynui_frame_renderer_tests PRIVATE rynui_renderer_sdl)
add_test(NAME rynui.frame_renderer COMMAND rynui_frame_renderer_tests)

add_executable(rynui_glyph_gpu_resources_tests glyph_gpu_resources_tests.cpp)
rynui_configure_cpp_target(rynui_glyph_gpu_resources_tests)
target_link_libraries(rynui_glyph_gpu_resources_tests PRIVATE rynui_renderer_common)
add_test(NAME rynui.glyph_gpu_resources COMMAND rynui_glyph_gpu_resources_tests)

add_executable(rynui_text_render_integration_tests text_render_integration_tests.cpp)
rynui_configure_cpp_target(rynui_text_render_integration_tests)
target_link_libraries(rynui_text_render_integration_tests PRIVATE rynui_renderer_common)
target_compile_definitions(
    rynui_text_render_integration_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.text_render_integration COMMAND rynui_text_render_integration_tests)

add_executable(rynui_text_scene_service_tests text_scene_service_tests.cpp)
rynui_configure_cpp_target(rynui_text_scene_service_tests)
target_link_libraries(rynui_text_scene_service_tests PRIVATE rynui_renderer_common)
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

add_executable(rynui_typography_component_tests typography_component_tests.cpp)
rynui_configure_cpp_target(rynui_typography_component_tests)
target_link_libraries(rynui_typography_component_tests PRIVATE rynui_components)
target_compile_definitions(
    rynui_typography_component_tests
    PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\""
)
add_test(NAME rynui.typography_component COMMAND rynui_typography_component_tests)
add_executable(rynui_typography_interaction_tests typography_interaction_tests.cpp)
rynui_configure_cpp_target(rynui_typography_interaction_tests)
target_link_libraries(rynui_typography_interaction_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_typography_interaction_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.typography_interaction COMMAND rynui_typography_interaction_tests)
add_executable(rynui_divider_component_tests divider_component_tests.cpp)
rynui_configure_cpp_target(rynui_divider_component_tests)
target_link_libraries(rynui_divider_component_tests PRIVATE rynui_components rynui_text_input_session)
target_compile_definitions(rynui_divider_component_tests PRIVATE
    "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
    "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
add_test(NAME rynui.divider_component COMMAND rynui_divider_component_tests)
add_executable(rynui_divider_public_api_tests divider_public_api_tests.cpp)
rynui_configure_cpp_target(rynui_divider_public_api_tests)
target_link_libraries(rynui_divider_public_api_tests PRIVATE RynUI::RynUI)
add_test(NAME rynui.divider_public_api COMMAND rynui_divider_public_api_tests)

add_executable(rynui_text_component_frame_tests text_component_frame_tests.cpp)
rynui_configure_cpp_target(rynui_text_component_frame_tests)
target_link_libraries(
    rynui_text_component_frame_tests
    PRIVATE
        rynui_components
        rynui_renderer_common
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
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windows_text_evidence.cmake"
)

add_test(
    NAME rynui.linux_text_evidence
    COMMAND ${CMAKE_COMMAND}
        "-DEVIDENCE_REPORT=${PROJECT_SOURCE_DIR}/openspec/changes/003-20260827-build-text-component-foundation/evidence/linux-gcc.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_linux_text_evidence.cmake"
)

add_test(
    NAME rynui.generated_shaders
    COMMAND ${CMAKE_COMMAND}
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_generated_shaders.cmake"
)

add_executable(rynui_embedded_shader_tests embedded_shader_tests.cpp)
rynui_configure_cpp_target(rynui_embedded_shader_tests)
target_include_directories(rynui_embedded_shader_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_embedded_shader_tests PRIVATE rynui_renderer_sdl)
rynui_embed_target_shaders(rynui_embedded_shader_tests)
add_test(NAME rynui.embedded_shaders COMMAND rynui_embedded_shader_tests "${RYNUI_GENERATED_SHADER_DIRECTORY}")

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
        "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DSHADER_SOURCE=${PROJECT_SOURCE_DIR}/shaders/rounded_effect.hlsl"
        "-DSHADER_LOCK=${PROJECT_SOURCE_DIR}/shaders/rounded_effect.lock.yaml"
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_rounded_effect_shader_contract.cmake"
)

add_test(NAME rynui.effect_paint_shader_contract
    COMMAND ${CMAKE_COMMAND}
        "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_effect_paint_shader_contract.cmake")
add_test(NAME rynui.effect_paint_shader_layout
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/verify_effect_paint_spirv.py"
        "${RYNUI_GENERATED_SHADER_DIRECTORY}"
        --negative-test "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/layout")
add_test(NAME rynui.effect_paint_shader_closure
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/effect_shader_closure.py"
        --root "${PROJECT_SOURCE_DIR}"
        --negative-test "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/closure")

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

if(RYNUI_HAS_SDL_GPU)
    add_executable(rynui_effect_paint_native_readback_tests effect_paint_native_readback_tests.cpp)
    rynui_configure_cpp_target(rynui_effect_paint_native_readback_tests)
    target_link_libraries(rynui_effect_paint_native_readback_tests PRIVATE
        rynui_renderer_sdl rynui_renderer_common SDL3::SDL3)
    target_compile_definitions(rynui_effect_paint_native_readback_tests PRIVATE
        "RYNUI_EFFECT_SHADER_LOCK=\"${PROJECT_SOURCE_DIR}/shaders/rounded_effect.lock.yaml\""
        "RYNUI_TEST_CONFIG=\"$<CONFIG>\"")
    rynui_embed_target_shaders(rynui_effect_paint_native_readback_tests)
    if(WIN32)
        add_test(NAME rynui.effect_paint_d3d12_hidden
            COMMAND rynui_effect_paint_native_readback_tests --expect-driver direct3d12
                --suite transport
                --output "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/rgba")
        set_tests_properties(rynui.effect_paint_d3d12_hidden PROPERTIES
            ENVIRONMENT "SDL_GPU_DRIVER=direct3d12" LABELS "windows;gpu;hidden" TIMEOUT 60 RUN_SERIAL TRUE)
        add_test(NAME rynui.effect_cap_d3d12_hidden
            COMMAND rynui_effect_paint_native_readback_tests --expect-driver direct3d12 --suite caps
                --output "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/caps-rgba")
        # Parent/Astra must resolve the finite large-radius counterexample before
        # enabling the capability and this independent numerical acceptance.
        set_tests_properties(rynui.effect_cap_d3d12_hidden PROPERTIES DISABLED TRUE
            ENVIRONMENT "SDL_GPU_DRIVER=direct3d12" LABELS "windows;gpu;hidden;pending-cap-numerics" TIMEOUT 60 RUN_SERIAL TRUE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME rynui.effect_paint_vulkan_private
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_background_wayland.py"
                --timeout 60
                --log-dir "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/wayland"
                -- "$<TARGET_FILE:rynui_effect_paint_native_readback_tests>" --expect-driver vulkan
                --suite transport
                --output "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/rgba")
        set_tests_properties(rynui.effect_paint_vulkan_private PROPERTIES
            ENVIRONMENT "SDL_GPU_DRIVER=vulkan" LABELS "linux;gpu;private-wayland" TIMEOUT 80 RUN_SERIAL TRUE)
        add_test(NAME rynui.effect_cap_vulkan_private
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_background_wayland.py"
                --timeout 60
                --log-dir "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/caps-wayland"
                -- "$<TARGET_FILE:rynui_effect_paint_native_readback_tests>" --expect-driver vulkan --suite caps
                --output "${PROJECT_SOURCE_DIR}/out/107-progress-b2/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/caps-rgba")
        set_tests_properties(rynui.effect_cap_vulkan_private PROPERTIES DISABLED TRUE
            ENVIRONMENT "SDL_GPU_DRIVER=vulkan" LABELS "linux;gpu;private-wayland;pending-cap-numerics" TIMEOUT 80 RUN_SERIAL TRUE)
    endif()
    add_executable(rynui_image_rounding_gpu_tests image_rounding_gpu_tests.cpp)
    rynui_configure_cpp_target(rynui_image_rounding_gpu_tests)
    target_link_libraries(rynui_image_rounding_gpu_tests PRIVATE rynui_components rynui_renderer_sdl rynui_renderer_common rynui_image SDL3::SDL3)
    target_compile_definitions(rynui_image_rounding_gpu_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    rynui_embed_target_shaders(rynui_image_rounding_gpu_tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME rynui.image_rounding_vulkan_private
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_background_wayland.py"
                --log-dir "${PROJECT_SOURCE_DIR}/out/image-rounding/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/wayland"
                --timeout 60 -- "$<TARGET_FILE:rynui_image_rounding_gpu_tests>"
                "${PROJECT_SOURCE_DIR}/out/image-rounding/${RYNUI_CONFIGURE_PRESET}/$<CONFIG>/readback")
        set_tests_properties(rynui.image_rounding_vulkan_private PROPERTIES
            LABELS "linux;gpu;private-wayland" TIMEOUT 80 RUN_SERIAL TRUE)
    endif()
    add_executable(rynui_paint_opacity_gpu_tests paint_opacity_gpu_tests.cpp)
    rynui_configure_cpp_target(rynui_paint_opacity_gpu_tests)
    target_link_libraries(rynui_paint_opacity_gpu_tests PRIVATE rynui_components rynui_renderer_sdl rynui_renderer_common SDL3::SDL3)
    target_compile_definitions(rynui_paint_opacity_gpu_tests PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    rynui_embed_target_shaders(rynui_paint_opacity_gpu_tests)
    add_executable(rynui_image_gpu_tests image_gpu_tests.cpp)
    rynui_configure_cpp_target(rynui_image_gpu_tests)
    target_link_libraries(rynui_image_gpu_tests PRIVATE rynui_renderer_sdl rynui_renderer_common rynui_image SDL3::SDL3)
    rynui_embed_target_shaders(rynui_image_gpu_tests)
endif()
