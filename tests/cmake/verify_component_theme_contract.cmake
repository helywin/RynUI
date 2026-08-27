cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR "${RYNUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

set(component_sources
    "${RYNUI_SOURCE_DIR}/src/component/text_component.cpp"
    "${RYNUI_SOURCE_DIR}/src/component/text_component.hpp"
    "${RYNUI_SOURCE_DIR}/src/component/button_component.cpp"
    "${RYNUI_SOURCE_DIR}/src/component/button_component.hpp"
    "${RYNUI_SOURCE_DIR}/src/component/flex_component.cpp"
    "${RYNUI_SOURCE_DIR}/src/component/space_component.cpp"
    "${RYNUI_SOURCE_DIR}/src/component/layout_container_values.hpp"
)

set(all_component_source "")
foreach(source IN LISTS component_sources)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Theme-migrated component source is missing: ${source}")
    endif()
    file(READ "${source}" content)
    string(APPEND all_component_source "\n${content}")
    foreach(forbidden IN ITEMS
            "DefaultThemeSnapshot"
            "default_theme_snapshot"
            "resolve_theme("
            "ButtonVisualLayer::focus_ring")
        string(FIND "${content}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "Stable component bypassed mounted Theme tokens with: ${forbidden} in ${source}")
        endif()
    endforeach()
endforeach()

foreach(required IN ITEMS
        "theme->text_font_family()"
        "theme->text_font_size()"
        "theme->text_line_height()"
        "theme->button_colors()"
        "theme->button_shadows()"
        "theme->focus_outline_width()"
        "theme->focus_outline_offset()"
        "theme.layout_gap_small()"
        "theme.layout_gap_middle()"
        "theme.layout_gap_large()")
    string(FIND "${all_component_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Theme component contract is missing accessor: ${required}")
    endif()
endforeach()
