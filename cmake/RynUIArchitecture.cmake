include_guard(GLOBAL)

function(rynui_verify_build_contract)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
        message(FATAL_ERROR
            "RynUI official builds require the Ninja Multi-Config generator. "
            "Configure with a repository CMake preset.")
    endif()

    if(WIN32 AND NOT MSVC)
        message(FATAL_ERROR
            "RynUI Windows builds require MSVC. Enter a Visual Studio Developer "
            "Environment before using the windows-msvc preset.")
    endif()

    if(RYNUI_EXPECTED_TOOLCHAIN STREQUAL "msvc" AND NOT MSVC)
        message(FATAL_ERROR "The selected preset requires MSVC.")
    elseif(RYNUI_EXPECTED_TOOLCHAIN STREQUAL "gcc"
            AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "The selected preset requires GCC.")
    elseif(RYNUI_EXPECTED_TOOLCHAIN STREQUAL "clang"
            AND NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
        message(FATAL_ERROR "The selected preset requires Clang.")
    endif()
endfunction()

function(rynui_assert_no_link_dependency_matching target forbidden_pattern)
    foreach(property_name IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_property(property_is_set TARGET ${target} PROPERTY ${property_name} SET)
        if(property_is_set)
            get_target_property(link_dependencies ${target} ${property_name})
            foreach(link_dependency IN LISTS link_dependencies)
                if(link_dependency MATCHES "${forbidden_pattern}")
                    message(FATAL_ERROR
                        "Target ${target} must not link build-time dependency "
                        "${link_dependency}.")
                endif()
            endforeach()
        endif()
    endforeach()
endfunction()

function(rynui_configure_cpp_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(rynui_assert_direct_dependencies target)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(property_name INTERFACE_LINK_LIBRARIES)
    else()
        set(property_name LINK_LIBRARIES)
    endif()

    get_property(property_is_set TARGET ${target} PROPERTY ${property_name} SET)
    if(property_is_set)
        get_target_property(actual_dependencies ${target} ${property_name})
    else()
        set(actual_dependencies "")
    endif()

    set(expected_dependencies ${ARGN})
    list(SORT actual_dependencies)
    list(SORT expected_dependencies)

    if(NOT "${actual_dependencies}" STREQUAL "${expected_dependencies}")
        message(FATAL_ERROR
            "Invalid direct dependencies for ${target}: expected "
            "[${expected_dependencies}], got [${actual_dependencies}].")
    endif()
endfunction()

function(rynui_verify_public_api public_include_directory)
    file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS
        "${public_include_directory}/*.h"
        "${public_include_directory}/*.hpp")

    foreach(public_header IN LISTS public_headers)
        file(READ "${public_header}" header_contents)
        string(REGEX MATCH
            "(class|struct|using|typedef)[ \t\r\n]+Modifier([^A-Za-z0-9_]|$)"
            forbidden_modifier "${header_contents}")
        if(forbidden_modifier)
            message(FATAL_ERROR
                "Public header ${public_header} exposes a generic Modifier type. "
                "Use typed Props, typed slots, LayoutStyle, and Theme tokens instead.")
        endif()

        foreach(forbidden_include IN ITEMS
                "ft2build\\.h"
                "freetype/"
                "hb\\.h"
                "hb-ft\\.h"
                "SDL3/"
                "src/"
                "font/"
                "graphics/"
                "layout/"
                "platform/"
                "renderer/"
                "runtime/")
            if(header_contents MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*${forbidden_include}")
                message(FATAL_ERROR
                    "Public header ${public_header} exposes forbidden third-party "
                    "include matching ${forbidden_include}.")
            endif()
        endforeach()

        if(public_header MATCHES "/prop\\.hpp$")
            foreach(forbidden_prop_symbol IN ITEMS
                    Observer
                    ReactiveSource
                    NodeId
                    Layout
                    FreeType
                    HarfBuzz
                    SDL)
                if(header_contents MATCHES "${forbidden_prop_symbol}")
                    message(FATAL_ERROR
                        "Public Prop header ${public_header} exposes forbidden "
                        "symbol ${forbidden_prop_symbol}.")
                endif()
            endforeach()
        endif()

        if(public_header MATCHES "/component\\.hpp$")
            foreach(forbidden_component_symbol IN ITEMS
                    ComponentHost
                    MountContext
                    NodeId
                    Layout
                    Scene
                    GPU
                    SDL)
                if(header_contents MATCHES "${forbidden_component_symbol}")
                    message(FATAL_ERROR
                        "Public component header ${public_header} exposes forbidden "
                        "symbol ${forbidden_component_symbol}.")
                endif()
            endforeach()
        endif()

        if(public_header MATCHES "/layout_style\\.hpp$")
            foreach(forbidden_layout_style_symbol IN ITEMS
                    Color
                    Font
                    Padding
                    Background
                    Modifier
                    PrimitiveStyle
                    NodeId
                    SDL)
                if(header_contents MATCHES "${forbidden_layout_style_symbol}")
                    message(FATAL_ERROR
                        "Public LayoutStyle header ${public_header} exposes forbidden "
                        "symbol ${forbidden_layout_style_symbol}.")
                endif()
            endforeach()
        endif()

        if(public_header MATCHES "/text\\.hpp$")
            foreach(forbidden_text_symbol IN ITEMS
                    ComponentHost
                    NodeId
                    Font
                    HarfBuzz
                    FreeType
                    Scene
                    GPU
                    SDL
                    PrimitiveStyle
                    Color)
                if(header_contents MATCHES "${forbidden_text_symbol}")
                    message(FATAL_ERROR
                        "Public Text header ${public_header} exposes forbidden "
                        "symbol ${forbidden_text_symbol}.")
                endif()
            endforeach()
        endif()
    endforeach()
endfunction()
