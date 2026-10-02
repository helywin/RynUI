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

foreach(name IN ITEMS logical_scene_packing quad_primitive binding_quad_integration button_scene_service
        font_runtime text_engine text_state text_scene_service
        text_render_integration glyph_atlas glyph_scene glyph_gpu_resources
        rounded_effect_gpu_resources input_component input_gpu input_caret_blink text_component
        button_component selection_component radio_component typography_component
        typography_interaction divider_component text_component_frame frame_scheduler)
    rynui_portable_test(${name} "${name}_tests.cpp")
endforeach()

if(TARGET rynui_renderer_recording)
    rynui_portable_test(scene_backend "scene_backend_tests.cpp")
    target_link_libraries(rynui_portable_scene_backend PRIVATE rynui_renderer_recording)
endif()

add_test(NAME rynui.portable.backend_boundaries
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/portable/verify_boundaries.py"
        --source "${PROJECT_SOURCE_DIR}" --build "${CMAKE_BINARY_DIR}")
