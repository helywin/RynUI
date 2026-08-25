add_executable(rynui_minimal minimal/main.cpp)
rynui_configure_cpp_target(rynui_minimal)
target_link_libraries(rynui_minimal PRIVATE RynUI::RynUI rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_minimal "SDL3_shadercross")

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
