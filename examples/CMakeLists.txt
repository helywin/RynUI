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
target_link_libraries(
    rynui_text_demo
    PRIVATE rynui_components rynui_default_fonts rynui_renderer_sdl
)
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
    rounded_effect.vertex.dxil
    rounded_effect.vertex.spv
    rounded_effect.fragment.dxil
    rounded_effect.fragment.spv
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
target_link_libraries(
    rynui_button_demo
    PRIVATE rynui_components rynui_default_fonts rynui_renderer_sdl
)
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
    rounded_effect.vertex.dxil
    rounded_effect.vertex.spv
    rounded_effect.fragment.dxil
    rounded_effect.fragment.spv
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
target_link_libraries(
    rynui_layout_demo
    PRIVATE rynui_components rynui_default_fonts rynui_renderer_sdl
)
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
    rounded_effect.vertex.dxil
    rounded_effect.vertex.spv
    rounded_effect.fragment.dxil
    rounded_effect.fragment.spv
)
add_custom_command(TARGET rynui_layout_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_layout_demo>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_layout_demo>/fonts/cjk.otf"
)

file(SHA256
    "${PROJECT_SOURCE_DIR}/design-tokens/ant-design/6.5.0/catalog.yaml"
    rynui_token_catalog_sha256
)
if(WIN32)
    set(rynui_token_gallery_window_system "win32")
elseif(UNIX AND NOT APPLE)
    set(rynui_token_gallery_window_system "wayland-or-x11")
else()
    set(rynui_token_gallery_window_system "unsupported")
endif()

add_library(
    rynui_token_gallery_definition STATIC
    token_gallery/token_gallery_definition.cpp
)
rynui_configure_cpp_target(rynui_token_gallery_definition)
target_include_directories(
    rynui_token_gallery_definition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/token_gallery"
)
target_link_libraries(rynui_token_gallery_definition PUBLIC RynUI::RynUI)

add_executable(
    rynui_token_gallery
    token_gallery/main.cpp
    token_gallery/token_gallery_runtime.cpp
)
rynui_configure_cpp_target(rynui_token_gallery)
target_link_libraries(
    rynui_token_gallery
    PRIVATE
        rynui_token_gallery_definition
        rynui_components
        rynui_default_fonts
        rynui_renderer_sdl
)
target_compile_definitions(
    rynui_token_gallery
    PRIVATE
        "RYNUI_TOKEN_CATALOG_HASH=\"${rynui_token_catalog_sha256}\""
        "RYNUI_WINDOW_SYSTEM=\"${rynui_token_gallery_window_system}\""
)
rynui_assert_no_link_dependency_matching(rynui_token_gallery "[Ss]hader[Cc]ross")
add_custom_command(TARGET rynui_token_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts"
)
rynui_deploy_target_shaders(rynui_token_gallery
    quad.vertex.dxil
    quad.vertex.spv
    quad.fragment.dxil
    quad.fragment.spv
    glyph.vertex.dxil
    glyph.vertex.spv
    glyph.fragment.dxil
    glyph.fragment.spv
    rounded_effect.vertex.dxil
    rounded_effect.vertex.spv
    rounded_effect.fragment.dxil
    rounded_effect.fragment.spv
)
add_custom_command(TARGET rynui_token_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts/cjk.otf"
)
