# CPU-only Gallery support is shared by native examples and HEADLESS tests.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
file(GLOB rynui_gallery_public_headers CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/ryn/*.hpp")
set(rynui_gallery_source_output "${CMAKE_CURRENT_BINARY_DIR}/gallery-generated/generated_gallery_source.inc")
add_custom_command(
    OUTPUT "${rynui_gallery_source_output}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/generate_gallery_source.py"
        --root "${PROJECT_SOURCE_DIR}" --output "${rynui_gallery_source_output}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_gallery_source.py"
        "${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
        ${rynui_gallery_public_headers}
    VERBATIM
)

add_library(
    rynui_token_gallery_definition STATIC
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/ant_design_reference_catalog.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/reference_surface.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_document_model.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_page_model.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_test_inventory.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_sample_source.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_source_panel.cpp"
    "${rynui_gallery_source_output}"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/token_gallery_definition.cpp"
)
rynui_configure_cpp_target(rynui_token_gallery_definition)
target_compile_options(rynui_token_gallery_definition PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_include_directories(
    rynui_token_gallery_definition
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/token_gallery"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gallery-generated"
)
target_link_libraries(rynui_token_gallery_definition PUBLIC RynUI::RynUI)

add_library(
    rynui_token_gallery_reference_surface STATIC
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_page_controller.cpp"
)

add_library(
    rynui_token_gallery_document_viewport STATIC
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_document_viewport.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_page_viewport.cpp"
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_scroll_region.cpp"
)
rynui_configure_cpp_target(rynui_token_gallery_document_viewport)
target_include_directories(
    rynui_token_gallery_document_viewport
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/token_gallery"
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
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/token_gallery"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    rynui_token_gallery_reference_surface
    PUBLIC rynui_token_gallery_definition
    PRIVATE rynui_components
)

add_library(rynui_gallery_pages_host STATIC
    "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_pages_host.cpp")
rynui_configure_cpp_target(rynui_gallery_pages_host)
target_compile_options(rynui_gallery_pages_host PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_include_directories(rynui_gallery_pages_host PUBLIC "${PROJECT_SOURCE_DIR}/examples/token_gallery"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(rynui_gallery_pages_host PUBLIC
    rynui_token_gallery_reference_surface rynui_token_gallery_document_viewport PRIVATE rynui_components)

add_library(rynui_gallery_page_acceptance STATIC "${PROJECT_SOURCE_DIR}/examples/token_gallery/gallery_page_acceptance.cpp")
rynui_configure_cpp_target(rynui_gallery_page_acceptance)
target_link_libraries(rynui_gallery_page_acceptance PUBLIC rynui_token_gallery_definition)
