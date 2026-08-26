cmake_minimum_required(VERSION 3.25)

project(RynUITextDependencyContract LANGUAGES CXX)

if(NOT DEFINED RYNUI_ROOT OR "${RYNUI_ROOT}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_ROOT is required.")
endif()

list(APPEND CMAKE_MODULE_PATH "${RYNUI_ROOT}/cmake")
include(RynUIDependencies)

rynui_resolve_text_dependencies()
rynui_resolve_validation_fonts(contract_latin_font contract_cjk_font)

if(NOT TARGET RynUI::FreeType OR NOT TARGET RynUI::HarfBuzz)
    message(FATAL_ERROR "Canonical text dependency targets were not created.")
endif()

get_target_property(freetype_links
    rynui_freetype_dependency INTERFACE_LINK_LIBRARIES)
if(NOT "Freetype::Freetype" IN_LIST freetype_links)
    message(FATAL_ERROR "RynUI::FreeType does not forward Freetype::Freetype.")
endif()

get_target_property(harfbuzz_links
    rynui_harfbuzz_dependency INTERFACE_LINK_LIBRARIES)
if(NOT "harfbuzz::harfbuzz" IN_LIST harfbuzz_links)
    message(FATAL_ERROR "RynUI::HarfBuzz does not forward harfbuzz::harfbuzz.")
endif()

add_library(text_dependency_consumer INTERFACE)
target_link_libraries(text_dependency_consumer
    INTERFACE RynUI::FreeType RynUI::HarfBuzz)
