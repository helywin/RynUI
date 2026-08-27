add_library(rynui_string STATIC
    text/string.cpp
)
rynui_configure_cpp_target(rynui_string)
target_include_directories(rynui_string PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

add_library(rynui_font STATIC
    font/font_runtime.cpp
)
rynui_configure_cpp_target(rynui_font)
target_include_directories(rynui_font PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(rynui_font PRIVATE RynUI::FreeType RynUI::HarfBuzz)

add_library(rynui_text STATIC
    text/text_engine.cpp
)
rynui_configure_cpp_target(rynui_text)
target_include_directories(rynui_text PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(rynui_text PUBLIC rynui_font rynui_string)

add_library(rynui_reactive STATIC
    reactive/reactive.cpp
)
rynui_configure_cpp_target(rynui_reactive)
target_include_directories(rynui_reactive PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(rynui_reactive PRIVATE
    RYNUI_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    RYNUI_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    RYNUI_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

add_library(rynui_runtime STATIC
    runtime/component_host.cpp
    runtime/component_mount.cpp
    runtime/frame_scheduler.cpp
    runtime/invalidation.cpp
    runtime/layout_style_adapter.cpp
    runtime/node_store.cpp
    runtime/runtime.cpp
)
rynui_configure_cpp_target(rynui_runtime)
target_include_directories(rynui_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rynui_runtime PRIVATE rynui_reactive)

add_library(rynui_layout STATIC
    layout/layout_engine.cpp
    layout/layout.cpp
)
rynui_configure_cpp_target(rynui_layout)
target_include_directories(rynui_layout PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rynui_layout PRIVATE rynui_runtime)

add_library(rynui_graphics STATIC
    graphics/glyph_atlas.cpp
    graphics/glyph_scene.cpp
    graphics/graphics.cpp
    graphics/quad_primitive.cpp
    graphics/quad_scene.cpp
)
rynui_configure_cpp_target(rynui_graphics)
target_include_directories(rynui_graphics PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rynui_graphics PRIVATE rynui_font rynui_layout rynui_text)

add_library(rynui_text_scene STATIC
    text/text_scene_service.cpp
)
rynui_configure_cpp_target(rynui_text_scene)
target_include_directories(rynui_text_scene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rynui_text_scene PRIVATE
    rynui_graphics
    rynui_runtime
    rynui_text
)

add_library(rynui_platform_sdl STATIC
    platform/sdl/platform_state.cpp
)
rynui_configure_cpp_target(rynui_platform_sdl)
target_include_directories(rynui_platform_sdl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rynui_platform_sdl PRIVATE SDL3::SDL3)

add_library(rynui_renderer_sdl STATIC
    renderer/sdl/frame_renderer.cpp
    renderer/sdl/glyph_gpu_resources.cpp
    renderer/sdl/quad_renderer.cpp
    renderer/sdl/scene_renderer.cpp
    renderer/sdl/text_render_controller.cpp
)
rynui_configure_cpp_target(rynui_renderer_sdl)
target_include_directories(rynui_platform_sdl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(rynui_renderer_sdl PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(rynui_renderer_sdl PRIVATE
    rynui_graphics
    rynui_platform_sdl
    rynui_runtime
    rynui_text
    rynui_text_scene
    SDL3::SDL3
)

add_library(rynui INTERFACE)
add_library(RynUI::RynUI ALIAS rynui)
target_compile_features(rynui INTERFACE cxx_std_20)
target_include_directories(rynui INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_link_libraries(rynui INTERFACE rynui_graphics rynui_reactive rynui_string)

rynui_assert_direct_dependencies(rynui_string)
rynui_assert_direct_dependencies(rynui_font RynUI::FreeType RynUI::HarfBuzz)
rynui_assert_direct_dependencies(rynui_text rynui_font rynui_string)
rynui_assert_direct_dependencies(rynui_reactive)
rynui_assert_direct_dependencies(rynui_runtime rynui_reactive)
rynui_assert_direct_dependencies(rynui_layout rynui_runtime)
rynui_assert_direct_dependencies(rynui_graphics rynui_font rynui_layout rynui_text)
rynui_assert_direct_dependencies(
    rynui_text_scene
    rynui_graphics
    rynui_runtime
    rynui_text
)
rynui_assert_direct_dependencies(rynui_platform_sdl SDL3::SDL3)
rynui_assert_direct_dependencies(
    rynui_renderer_sdl
    SDL3::SDL3
    rynui_graphics
    rynui_platform_sdl
    rynui_runtime
    rynui_text
    rynui_text_scene
)
rynui_assert_direct_dependencies(rynui rynui_graphics rynui_reactive rynui_string)
