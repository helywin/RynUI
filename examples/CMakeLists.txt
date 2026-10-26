add_executable(rynui_minimal minimal/main.cpp)
rynui_configure_cpp_target(rynui_minimal)
target_link_libraries(rynui_minimal PRIVATE RynUI::RynUI rynui_renderer_sdl)
rynui_assert_no_link_dependency_matching(rynui_minimal "[Ss]hader[Cc]ross")

rynui_embed_target_shaders(rynui_minimal)

add_executable(rynui_text_demo text/main.cpp)
rynui_configure_cpp_target(rynui_text_demo)
target_link_libraries(
    rynui_text_demo
    PRIVATE rynui_components rynui_default_fonts rynui_renderer_sdl
)
rynui_assert_no_link_dependency_matching(rynui_text_demo "[Ss]hader[Cc]ross")
rynui_embed_target_shaders(rynui_text_demo)

add_executable(rynui_button_demo button/main.cpp)
rynui_configure_cpp_target(rynui_button_demo)
target_link_libraries(
    rynui_button_demo
    PRIVATE rynui_components rynui_default_fonts rynui_renderer_sdl
)
rynui_assert_no_link_dependency_matching(rynui_button_demo "[Ss]hader[Cc]ross")
rynui_embed_target_shaders(rynui_button_demo)

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
rynui_embed_target_shaders(rynui_layout_demo)

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
    token_gallery/ant_design_reference_catalog.cpp
    token_gallery/gallery_document_model.cpp
    token_gallery/token_gallery_definition.cpp
)
rynui_configure_cpp_target(rynui_token_gallery_definition)
target_compile_options(rynui_token_gallery_definition PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_include_directories(
    rynui_token_gallery_definition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/token_gallery"
)
target_link_libraries(rynui_token_gallery_definition PUBLIC RynUI::RynUI)

add_library(
    rynui_token_gallery_reference_surface STATIC
    token_gallery/reference_surface.cpp
)

add_library(
    rynui_token_gallery_document_viewport STATIC
    token_gallery/gallery_document_viewport.cpp
    token_gallery/gallery_scroll_region.cpp
)
rynui_configure_cpp_target(rynui_token_gallery_document_viewport)
target_include_directories(
    rynui_token_gallery_document_viewport
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/token_gallery"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_token_gallery_document_viewport
    PUBLIC rynui_token_gallery_definition
    PRIVATE rynui_runtime
)
rynui_configure_cpp_target(rynui_token_gallery_reference_surface)
target_include_directories(
    rynui_token_gallery_reference_surface
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/token_gallery"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_token_gallery_reference_surface
    PUBLIC rynui_token_gallery_definition
    PRIVATE rynui_components
)

add_executable(
    rynui_token_gallery
    token_gallery/main.cpp
    token_gallery/token_gallery_runtime.cpp
    token_gallery/typography_acceptance.cpp
    token_gallery/select_acceptance.cpp
    token_gallery/card_acceptance.cpp
    token_gallery/tabs_acceptance.cpp
    token_gallery/modal_acceptance.cpp
    token_gallery/slider_acceptance.cpp
    token_gallery/tooltip_acceptance.cpp
    token_gallery/button_acceptance.cpp
    token_gallery/divider_acceptance.cpp
    token_gallery/switch_acceptance.cpp
    token_gallery/checkbox_acceptance.cpp
    token_gallery/radio_acceptance.cpp
    token_gallery/flex_acceptance.cpp
    token_gallery/space_acceptance.cpp
    token_gallery/icon_acceptance.cpp
    token_gallery/input_acceptance.cpp
    token_gallery/text_area_acceptance.cpp
    token_gallery/otp_acceptance.cpp
    token_gallery/bidi_acceptance.cpp
    token_gallery/rounded_acceptance.cpp
)
rynui_configure_cpp_target(rynui_token_gallery)
target_link_libraries(
    rynui_token_gallery
    PRIVATE
        rynui_token_gallery_definition
        rynui_token_gallery_reference_surface
        rynui_token_gallery_document_viewport
        rynui_components
        rynui_default_fonts
        rynui_renderer_sdl
        SDL3::SDL3
)
target_compile_definitions(
    rynui_token_gallery
    PRIVATE
        "RYNUI_TOKEN_CATALOG_HASH=\"${rynui_token_catalog_sha256}\""
        "RYNUI_WINDOW_SYSTEM=\"${rynui_token_gallery_window_system}\""
)
rynui_assert_no_link_dependency_matching(rynui_token_gallery "[Ss]hader[Cc]ross")
rynui_embed_target_shaders(rynui_token_gallery)

# Explicit preparation for the fixed-font acceptance modes, outside the default build.
add_custom_target(rynui_token_gallery_acceptance_fonts
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_token_gallery>/fonts/cjk.otf"
    DEPENDS rynui_token_gallery
    VERBATIM)

add_executable(rynui_rounded_acceptance token_gallery/rounded_acceptance.cpp)
rynui_configure_cpp_target(rynui_rounded_acceptance)
target_compile_definitions(rynui_rounded_acceptance PRIVATE RYNUI_ROUNDED_ACCEPTANCE_STANDALONE)
target_link_libraries(rynui_rounded_acceptance PRIVATE
    rynui_components rynui_default_fonts rynui_renderer_sdl SDL3::SDL3)
rynui_assert_no_link_dependency_matching(rynui_rounded_acceptance "[Ss]hader[Cc]ross")
rynui_embed_target_shaders(rynui_rounded_acceptance)
add_custom_command(TARGET rynui_rounded_acceptance POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:rynui_rounded_acceptance>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RYNUI_VALIDATION_LATIN_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_rounded_acceptance>/fonts/latin.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RYNUI_VALIDATION_CJK_FONT_FILE}"
        "$<TARGET_FILE_DIR:rynui_rounded_acceptance>/fonts/cjk.otf")
