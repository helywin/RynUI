# Compile existing CPU/component fixtures against the same production targets.
# Native integration tests remain in the SDL configuration's full suite.
function(rynui_portable_test name source)
    add_executable(rynui_portable_${name} "${CMAKE_CURRENT_SOURCE_DIR}/../${source}")
    rynui_configure_cpp_target(rynui_portable_${name})
    target_link_libraries(rynui_portable_${name} PRIVATE
        rynui_components rynui_renderer_common rynui_text_input_session)
    target_compile_definitions(rynui_portable_${name} PRIVATE
        "RYNUI_VALIDATION_LATIN_FONT=\"${RYNUI_VALIDATION_LATIN_FONT_FILE}\""
        "RYNUI_VALIDATION_CJK_FONT=\"${RYNUI_VALIDATION_CJK_FONT_FILE}\"")
    add_test(NAME rynui.portable.${name} COMMAND rynui_portable_${name})
endfunction()

foreach(name IN ITEMS prop glyph_texture_upload texture_upload_batch_layout logical_scene_packing quad_primitive binding_quad_integration button_scene_service
        font_runtime text_engine text_state text_scene_service
        text_render_integration glyph_atlas glyph_scene glyph_gpu_resources icon_catalog icon_component icon_vector
        rounded_effect_math rounded_effect_store rounded_effect_scene rounded_effect_gpu
        rounded_effect_gpu_resources input_component input_variant input_count input_actions input_gpu input_caret_blink input_public_api password_component search_component search_public_api text_component
        button_component selection_component selection_controls_public_api switch_features switch_public_api checkbox_group checkbox_public_api theme_algorithm theme_runtime slider_component radio_component radio_features radio_public_api focus_order focus_state focus_lifecycle typography_component
        typography_interaction divider_component divider_public_api text_component_frame frame_scheduler window_overlay tooltip_component interaction_registry pointer_route text_input_session
        text_editor text_history text_reconcile text_clipboard_command input_display text_caret_map text_area)
    rynui_portable_test(${name} "${name}_tests.cpp")
endforeach()

foreach(name IN ITEMS layout_engine layout_style layout_style_public_api layout_allocation
        flex_component flex_features flex_public_api flex_header_isolation space_component space_features space_compact space_public_api space_header_isolation)
    rynui_portable_test(${name} "${name}_tests.cpp")
endforeach()

target_compile_definitions(rynui_portable_theme_algorithm PRIVATE
    "RYNUI_THEME_GOLDEN_DIRECTORY=\"${PROJECT_SOURCE_DIR}/design-tokens/ant-design/6.6.5/golden\"")

rynui_portable_test(rounded_effect_allocation "rounded_effect_allocation_benchmark.cpp")
rynui_portable_test(input_scene_allocation "input_scene_allocation_benchmark.cpp")
rynui_portable_test(text_editor_allocation "text_editor_allocation_benchmark.cpp")

if(TARGET rynui_renderer_recording)
    rynui_portable_test(scene_backend "scene_backend_tests.cpp")
    target_link_libraries(rynui_portable_scene_backend PRIVATE rynui_renderer_recording)
endif()

add_test(NAME rynui.portable.backend_boundaries
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/portable/verify_boundaries.py"
        --source "${PROJECT_SOURCE_DIR}" --build "${CMAKE_BINARY_DIR}")

add_test(NAME rynui.portable.icon_asset_contract
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/verify_icon_assets.py")
