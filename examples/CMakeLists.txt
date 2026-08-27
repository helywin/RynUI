add_executable(rynui_minimal minimal/main.cpp)
rynui_configure_cpp_target(rynui_minimal)
target_link_libraries(rynui_minimal PRIVATE RynUI::RynUI rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_minimal "[Ss]hader[Cc]ross")

add_dependencies(rynui_minimal rynui_shaders)
add_custom_command(TARGET rynui_minimal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_minimal>/shaders"
)
foreach(shader_file IN ITEMS
        quad.vertex.dxil
        quad.vertex.spv
        quad.fragment.dxil
        quad.fragment.spv)
    add_custom_command(TARGET rynui_minimal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RYNUI_GENERATED_SHADER_DIRECTORY}/${shader_file}"
            "$<TARGET_FILE_DIR:rynui_minimal>/shaders/${shader_file}"
    )
endforeach()

add_executable(rynui_text_demo text/main.cpp)
rynui_configure_cpp_target(rynui_text_demo)
target_link_libraries(rynui_text_demo PRIVATE rynui_components rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_text_demo "[Ss]hader[Cc]ross")
add_dependencies(rynui_text_demo rynui_shaders)
add_custom_command(TARGET rynui_text_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_text_demo>/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_text_demo>/fonts"
)
foreach(shader_file IN ITEMS
        quad.vertex.dxil
        quad.vertex.spv
        quad.fragment.dxil
        quad.fragment.spv
        glyph.vertex.dxil
        glyph.vertex.spv
        glyph.fragment.dxil
        glyph.fragment.spv)
    add_custom_command(TARGET rynui_text_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RYNUI_GENERATED_SHADER_DIRECTORY}/${shader_file}"
            "$<TARGET_FILE_DIR:rynui_text_demo>/shaders/${shader_file}"
    )
endforeach()
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
add_dependencies(rynui_button_demo rynui_shaders)
add_custom_command(TARGET rynui_button_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_button_demo>/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts"
)
foreach(shader_file IN ITEMS
        quad.vertex.dxil
        quad.vertex.spv
        quad.fragment.dxil
        quad.fragment.spv
        glyph.vertex.dxil
        glyph.vertex.spv
        glyph.fragment.dxil
        glyph.fragment.spv)
    add_custom_command(TARGET rynui_button_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RYNUI_GENERATED_SHADER_DIRECTORY}/${shader_file}"
            "$<TARGET_FILE_DIR:rynui_button_demo>/shaders/${shader_file}"
    )
endforeach()
add_custom_command(TARGET rynui_button_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_button_demo>/fonts/cjk.otf"
)
