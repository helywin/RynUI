add_executable(rynui_minimal minimal/main.cpp)
rynui_configure_cpp_target(rynui_minimal)
target_link_libraries(rynui_minimal PRIVATE RynUI::RynUI rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_minimal "[Ss]hader[Cc]ross")

rynui_deploy_target_shaders(rynui_minimal
    quad.vertex.dxil
    quad.vertex.spv
    quad.fragment.dxil
    quad.fragment.spv
)

add_executable(rynui_text_demo text/main.cpp)
rynui_configure_cpp_target(rynui_text_demo)
target_link_libraries(rynui_text_demo PRIVATE rynui_components rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_text_demo "[Ss]hader[Cc]ross")
add_custom_command(TARGET rynui_text_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_text_demo>/fonts"
)
rynui_deploy_target_shaders(rynui_text_demo
    quad.vertex.dxil
    quad.vertex.spv
    quad.fragment.dxil
    quad.fragment.spv
    glyph.vertex.dxil
    glyph.vertex.spv
    glyph.fragment.dxil
    glyph.fragment.spv
)
add_custom_command(TARGET rynui_text_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_text_demo>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_text_demo>/fonts/cjk.otf"
)

add_executable(rynui_button_demo button/main.cpp)
rynui_configure_cpp_target(rynui_button_demo)
target_link_libraries(rynui_button_demo PRIVATE rynui_components rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_button_demo "[Ss]hader[Cc]ross")
add_custom_command(TARGET rynui_button_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts"
)
rynui_deploy_target_shaders(rynui_button_demo
    quad.vertex.dxil
    quad.vertex.spv
    quad.fragment.dxil
    quad.fragment.spv
    glyph.vertex.dxil
    glyph.vertex.spv
    glyph.fragment.dxil
    glyph.fragment.spv
)
add_custom_command(TARGET rynui_button_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts/cjk.otf"
)

add_executable(
    rynui_layout_demo
    layout/main.cpp
    layout/layout_demo_runtime.cpp
)
rynui_configure_cpp_target(rynui_layout_demo)
target_link_libraries(rynui_layout_demo PRIVATE rynui_components rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_layout_demo "[Ss]hader[Cc]ross")
add_custom_command(TARGET rynui_layout_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_layout_demo>/fonts"
)
rynui_deploy_target_shaders(rynui_layout_demo
    quad.vertex.dxil
    quad.vertex.spv
    quad.fragment.dxil
    quad.fragment.spv
    glyph.vertex.dxil
    glyph.vertex.spv
    glyph.fragment.dxil
    glyph.fragment.spv
)
add_custom_command(TARGET rynui_layout_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_layout_demo>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_layout_demo>/fonts/cjk.otf"
)
