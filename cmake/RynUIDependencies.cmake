include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/dependencies/RynUIDependencyLock.cmake")

set(RYNUI_DEPENDENCY_MODE "BUNDLED" CACHE STRING
    "Third-party dependency source: BUNDLED or SYSTEM")
set_property(CACHE RYNUI_DEPENDENCY_MODE PROPERTY STRINGS BUNDLED SYSTEM)
set(RYNUI_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH
    "Runnable host SDL_shadercross CLI; required for SYSTEM and cross builds")
set(RYNUI_SYSTEM_LATIN_FONT_FILE "" CACHE FILEPATH
    "Explicit Latin validation font for SYSTEM dependency mode")
set(RYNUI_SYSTEM_CJK_FONT_FILE "" CACHE FILEPATH
    "Explicit CJK validation font for SYSTEM dependency mode")

function(rynui_verify_dependency_mode)
    set(valid_modes BUNDLED SYSTEM)
    if(NOT RYNUI_DEPENDENCY_MODE IN_LIST valid_modes)
        message(FATAL_ERROR
            "RYNUI_DEPENDENCY_MODE must be one of [BUNDLED, SYSTEM], got "
            "'${RYNUI_DEPENDENCY_MODE}'. Select BUNDLED for locked inputs or "
            "SYSTEM and provide compatible packages and validation fonts.")
    endif()
endfunction()

function(rynui_resolve_bundled_libdecor)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
            OR NOT RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        return()
    endif()

    if(TARGET RynUI::LibDecor)
        return()
    endif()

    include(FetchContent)
    include(ExternalProject)
    find_package(PkgConfig REQUIRED)
    find_program(RYNUI_MESON_EXECUTABLE meson REQUIRED)
    find_program(RYNUI_NINJA_EXECUTABLE ninja REQUIRED)
    find_program(RYNUI_WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
    find_program(RYNUI_PATCH_EXECUTABLE patch REQUIRED)

    foreach(program_variable IN ITEMS
            RYNUI_MESON_EXECUTABLE
            RYNUI_NINJA_EXECUTABLE
            RYNUI_WAYLAND_SCANNER_EXECUTABLE
            RYNUI_PATCH_EXECUTABLE)
        if(NOT EXISTS "${${program_variable}}" OR IS_DIRECTORY "${${program_variable}}")
            message(FATAL_ERROR
                "Patched bundled libdecor requires a runnable ${program_variable}; "
                "got '${${program_variable}}'.")
        endif()
    endforeach()

    pkg_check_modules(RYNUI_LIBDECOR_WAYLAND_CLIENT REQUIRED wayland-client>=1.18)
    pkg_check_modules(RYNUI_LIBDECOR_WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.15)
    pkg_check_modules(RYNUI_LIBDECOR_CAIRO REQUIRED cairo)
    pkg_check_modules(RYNUI_LIBDECOR_PANGOCAIRO REQUIRED pangocairo)
    pkg_check_modules(RYNUI_LIBDECOR_WAYLAND_CURSOR REQUIRED wayland-cursor)

    set(libdecor_resizing_patch
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/libdecor/0001-expose-resizing-state.patch")
    set(libdecor_configuration_patch
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/libdecor/0002-retain-configuration.patch")

    message(STATUS
        "Resolving patched libdecor ${RYNUI_LIBDECOR_VERSION} from the locked archive. "
        "For offline builds, set FETCHCONTENT_SOURCE_DIR_RYNUI_LIBDECOR to prepared source.")

    FetchContent_Declare(
        rynui_libdecor
        URL "${RYNUI_LIBDECOR_SOURCE_URL}"
        URL_HASH "SHA256=${RYNUI_LIBDECOR_SOURCE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        SOURCE_SUBDIR "__rynui_no_cmake_project__"
        PATCH_COMMAND
            "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=<SOURCE_DIR>"
            "-DPATCH_1=${libdecor_resizing_patch}"
            "-DPATCH_2=${libdecor_configuration_patch}"
            "-DEXPECTED_PATCH_SHA256_1=${RYNUI_LIBDECOR_RESIZING_PATCH_SHA256}"
            "-DEXPECTED_PATCH_SHA256_2=${RYNUI_LIBDECOR_CONFIGURATION_PATCH_SHA256}"
            "-DEXPECTED_VERSION_FILE=meson.build"
            "-DEXPECTED_VERSION_PATTERN=version: '0[.]2[.]5'"
            "-DPATCH_EXECUTABLE=${RYNUI_PATCH_EXECUTABLE}"
            "-DALLOW_ALREADY_APPLIED=ON"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplySourcePatches.cmake"
    )
    FetchContent_MakeAvailable(rynui_libdecor)

    set(libdecor_stage "${CMAKE_BINARY_DIR}/_deps/rynui-libdecor-stage")
    set(libdecor_build "${CMAKE_BINARY_DIR}/_deps/rynui-libdecor-build")
    set(libdecor_library "${libdecor_stage}/lib/libdecor-0.so")
    set(libdecor_plugin
        "${libdecor_stage}/lib/libdecor/plugins-1/libdecor-cairo.so")
    file(MAKE_DIRECTORY
        "${libdecor_stage}/include/libdecor-0"
        "${libdecor_stage}/lib/libdecor/plugins-1")

    ExternalProject_Add(
        rynui_libdecor_external
        SOURCE_DIR "${rynui_libdecor_SOURCE_DIR}"
        BINARY_DIR "${libdecor_build}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        CONFIGURE_COMMAND
            "${RYNUI_MESON_EXECUTABLE}" setup
            "${libdecor_build}" "${rynui_libdecor_SOURCE_DIR}"
            --prefix "${libdecor_stage}"
            --libdir lib
            --buildtype debugoptimized
            --default-library shared
            -Ddemo=false
            -Ddbus=disabled
            -Dgtk=disabled
        BUILD_COMMAND
            "${RYNUI_MESON_EXECUTABLE}" compile -C "${libdecor_build}"
        INSTALL_COMMAND
            "${RYNUI_MESON_EXECUTABLE}" install -C "${libdecor_build}"
        BUILD_BYPRODUCTS
            "${libdecor_library}"
            "${libdecor_plugin}"
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
        USES_TERMINAL_INSTALL TRUE
    )

    add_library(rynui_libdecor SHARED IMPORTED GLOBAL)
    set_target_properties(rynui_libdecor PROPERTIES
        IMPORTED_LOCATION "${libdecor_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${libdecor_stage}/include/libdecor-0"
    )
    add_dependencies(rynui_libdecor rynui_libdecor_external)
    add_library(RynUI::LibDecor ALIAS rynui_libdecor)

    set(RYNUI_LIBDECOR_STAGE_PREFIX "${libdecor_stage}" CACHE INTERNAL
        "Build-local patched libdecor staging prefix")
    set(RYNUI_LIBDECOR_PLUGIN_DIR
        "${libdecor_stage}/lib/libdecor/plugins-1" CACHE INTERNAL
        "Build-local patched libdecor plugin directory")
    set(RYNUI_LIBDECOR_BUILD_RPATH
        "${libdecor_stage}/lib" CACHE INTERNAL
        "Build RPATH for the build-local patched libdecor")
endfunction()

function(rynui_resolve_sdl3)
    rynui_verify_dependency_mode()

    if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        rynui_resolve_bundled_libdecor()
        include(FetchContent)

        # The bundled mode owns this SDL build and keeps it minimal. These
        # cache values intentionally apply only when RynUI resolves SDL itself.
        set(SDL_SHARED OFF CACHE BOOL "Build the SDL3 shared library" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Build the SDL3 static library" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test library" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE)
        set(SDL_UNINSTALL OFF CACHE BOOL "Add the SDL3 uninstall target" FORCE)

        message(STATUS
            "Resolving SDL3 ${RYNUI_SDL3_VERSION} from the locked archive. "
            "For offline builds, set FETCHCONTENT_SOURCE_DIR_SDL3 to prepared source.")

        FetchContent_Declare(
            SDL3
            URL "${RYNUI_SDL3_SOURCE_URL}"
            URL_HASH "SHA256=${RYNUI_SDL3_SOURCE_SHA256}"
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
        FetchContent_MakeAvailable(SDL3)
    else()
        find_package(SDL3 ${RYNUI_SDL3_VERSION} CONFIG REQUIRED COMPONENTS SDL3)
    endif()

    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR
            "SDL3 resolution in ${RYNUI_DEPENDENCY_MODE} mode did not define "
            "required target SDL3::SDL3. Verify the locked source or select a "
            "compatible SDL3 CMake package.")
    endif()
endfunction()

function(rynui_resolve_text_dependencies)
    rynui_verify_dependency_mode()

    if(TARGET RynUI::FreeType OR TARGET RynUI::HarfBuzz)
        if(TARGET RynUI::FreeType AND TARGET RynUI::HarfBuzz)
            return()
        endif()
        message(FATAL_ERROR
            "Text dependency resolution found only one canonical target. "
            "RynUI::FreeType and RynUI::HarfBuzz must be created together.")
    endif()

    if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        include(FetchContent)

        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared third-party libraries" FORCE)
        set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip third-party install rules" FORCE)

        set(FT_DISABLE_ZLIB ON CACHE BOOL "Use FreeType internal zlib" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable FreeType bzip2 support" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "Disable FreeType PNG support" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL
            "Disable FreeType HarfBuzz auto-hint integration to avoid a dependency cycle" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable FreeType Brotli support" FORCE)

        message(STATUS
            "Resolving FreeType ${RYNUI_FREETYPE_VERSION} from the locked archive. "
            "For offline builds, set FETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE.")
        FetchContent_Declare(
            rynui_freetype
            URL "${RYNUI_FREETYPE_SOURCE_URL}"
            URL_HASH "SHA256=${RYNUI_FREETYPE_SOURCE_SHA256}"
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
        FetchContent_MakeAvailable(rynui_freetype)
        if(NOT TARGET freetype)
            message(FATAL_ERROR
                "Locked FreeType source did not define required target freetype.")
        endif()

        set(HB_HAVE_CAIRO OFF CACHE BOOL "Disable HarfBuzz Cairo helpers" FORCE)
        set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable HarfBuzz FreeType helpers" FORCE)
        set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "Disable HarfBuzz Graphite2" FORCE)
        set(HB_HAVE_GLIB OFF CACHE BOOL "Disable HarfBuzz GLib helpers" FORCE)
        set(HB_HAVE_ICU OFF CACHE BOOL "Disable HarfBuzz ICU helpers" FORCE)
        set(HB_HAVE_GOBJECT OFF CACHE BOOL "Disable HarfBuzz GObject bindings" FORCE)
        set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "Disable HarfBuzz introspection" FORCE)
        set(HB_BUILD_UTILS OFF CACHE BOOL "Disable HarfBuzz utilities" FORCE)
        set(HB_BUILD_SUBSET OFF CACHE BOOL "Disable HarfBuzz subset library" FORCE)
        set(HB_BUILD_RASTER OFF CACHE BOOL "Disable HarfBuzz raster library" FORCE)
        set(HB_BUILD_VECTOR OFF CACHE BOOL "Disable HarfBuzz vector library" FORCE)
        set(HB_BUILD_GPU OFF CACHE BOOL "Disable HarfBuzz GPU library" FORCE)
        set(HB_BUILD_GPU_DEMO OFF CACHE STRING "Disable HarfBuzz GPU demo" FORCE)
        if(WIN32)
            set(HB_HAVE_UNISCRIBE OFF CACHE BOOL "Disable HarfBuzz Uniscribe" FORCE)
            set(HB_HAVE_GDI OFF CACHE BOOL "Disable HarfBuzz GDI helpers" FORCE)
            set(HB_HAVE_DIRECTWRITE OFF CACHE BOOL "Disable HarfBuzz DirectWrite" FORCE)
        endif()

        message(STATUS
            "Resolving HarfBuzz ${RYNUI_HARFBUZZ_VERSION} from the locked archive. "
            "For offline builds, set FETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ.")
        FetchContent_Declare(
            rynui_harfbuzz
            URL "${RYNUI_HARFBUZZ_SOURCE_URL}"
            URL_HASH "SHA256=${RYNUI_HARFBUZZ_SOURCE_SHA256}"
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
        FetchContent_MakeAvailable(rynui_harfbuzz)
        if(NOT TARGET harfbuzz)
            message(FATAL_ERROR
                "Locked HarfBuzz source did not define required target harfbuzz.")
        endif()

        get_target_property(harfbuzz_direct_links harfbuzz LINK_LIBRARIES)
        if(NOT "freetype" IN_LIST harfbuzz_direct_links)
            message(FATAL_ERROR
                "Locked HarfBuzz target must link FreeType for hb-ft support.")
        endif()
        get_target_property(freetype_direct_links freetype LINK_LIBRARIES)
        if("harfbuzz" IN_LIST freetype_direct_links)
            message(FATAL_ERROR
                "Locked FreeType target must not link HarfBuzz; the text dependency "
                "direction is FreeType -> HarfBuzz only.")
        endif()

        set(freetype_target freetype)
        set(harfbuzz_target harfbuzz)
    else()
        find_package(Freetype ${RYNUI_FREETYPE_VERSION} EXACT CONFIG REQUIRED)
        find_package(harfbuzz ${RYNUI_HARFBUZZ_VERSION} EXACT CONFIG REQUIRED)

        if(NOT TARGET Freetype::Freetype)
            message(FATAL_ERROR
                "SYSTEM FreeType package did not define required target "
                "Freetype::Freetype.")
        endif()
        if(NOT TARGET harfbuzz::harfbuzz)
            message(FATAL_ERROR
                "SYSTEM HarfBuzz package did not define required target "
                "harfbuzz::harfbuzz.")
        endif()

        set(freetype_target Freetype::Freetype)
        set(harfbuzz_target harfbuzz::harfbuzz)
    endif()

    add_library(rynui_freetype_dependency INTERFACE)
    add_library(RynUI::FreeType ALIAS rynui_freetype_dependency)
    target_link_libraries(rynui_freetype_dependency INTERFACE ${freetype_target})

    add_library(rynui_harfbuzz_dependency INTERFACE)
    add_library(RynUI::HarfBuzz ALIAS rynui_harfbuzz_dependency)
    target_link_libraries(rynui_harfbuzz_dependency INTERFACE ${harfbuzz_target})
endfunction()

function(rynui_resolve_platform_font_service)
    if(TARGET RynUI::PlatformFonts)
        return()
    endif()

    add_library(rynui_platform_font_service INTERFACE)
    add_library(RynUI::PlatformFonts ALIAS rynui_platform_font_service)
    if(WIN32)
        target_link_libraries(rynui_platform_font_service INTERFACE dwrite)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Fontconfig is an explicit Linux desktop platform service. It resolves
        # the user's active desktop font configuration and is not a source-mode
        # fallback for RynUI's locked FreeType/HarfBuzz dependencies.
        find_package(Fontconfig 2.13 REQUIRED)
        if(NOT TARGET Fontconfig::Fontconfig)
            message(FATAL_ERROR
                "Linux system font resolution requires Fontconfig::Fontconfig.")
        endif()
        target_link_libraries(
            rynui_platform_font_service INTERFACE Fontconfig::Fontconfig)
    endif()
endfunction()

function(rynui_download_locked_file label url sha256 output_path)
    get_filename_component(output_directory "${output_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")

    if(EXISTS "${output_path}")
        file(SHA256 "${output_path}" existing_sha256)
        if("${existing_sha256}" STREQUAL "${sha256}")
            return()
        endif()
        file(REMOVE "${output_path}")
    endif()

    message(STATUS "Downloading locked ${label} to the build tree.")
    file(DOWNLOAD
        "${url}"
        "${output_path}"
        EXPECTED_HASH "SHA256=${sha256}"
        TLS_VERIFY ON
        INACTIVITY_TIMEOUT 60
        STATUS download_status
        LOG download_log
    )
    list(GET download_status 0 download_result)
    list(GET download_status 1 download_message)
    if(NOT download_result EQUAL 0)
        file(REMOVE "${output_path}")
        message(FATAL_ERROR
            "Failed to download locked ${label}: ${download_message}\n${download_log}")
    endif()
endfunction()

function(rynui_require_system_font input_path label output_variable)
    if("${input_path}" STREQUAL "")
        message(FATAL_ERROR
            "RYNUI_DEPENDENCY_MODE=SYSTEM requires ${label}. "
            "Provide an explicit compatible validation font file.")
    endif()
    if(NOT EXISTS "${input_path}" OR IS_DIRECTORY "${input_path}")
        message(FATAL_ERROR "${label} is not a readable font file: ${input_path}")
    endif()
    get_filename_component(resolved_path "${input_path}" REALPATH)
    set(${output_variable} "${resolved_path}" PARENT_SCOPE)
endfunction()

function(rynui_resolve_validation_fonts out_latin_font out_cjk_font)
    rynui_verify_dependency_mode()

    if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        set(font_directory "${CMAKE_BINARY_DIR}/_deps/rynui-validation-fonts")
        set(latin_font "${font_directory}/NotoSans-Regular.ttf")
        set(cjk_font "${font_directory}/NotoSansCJKsc-Regular.otf")

        rynui_download_locked_file(
            "Noto Sans ${RYNUI_NOTO_SANS_VERSION}"
            "${RYNUI_NOTO_SANS_SOURCE_URL}"
            "${RYNUI_NOTO_SANS_SOURCE_SHA256}"
            "${latin_font}")
        rynui_download_locked_file(
            "Noto Sans CJK SC ${RYNUI_NOTO_SANS_CJK_SC_VERSION}"
            "${RYNUI_NOTO_SANS_CJK_SC_SOURCE_URL}"
            "${RYNUI_NOTO_SANS_CJK_SC_SOURCE_SHA256}"
            "${cjk_font}")
    else()
        rynui_require_system_font(
            "${RYNUI_SYSTEM_LATIN_FONT_FILE}"
            "RYNUI_SYSTEM_LATIN_FONT_FILE"
            latin_font)
        rynui_require_system_font(
            "${RYNUI_SYSTEM_CJK_FONT_FILE}"
            "RYNUI_SYSTEM_CJK_FONT_FILE"
            cjk_font)
    endif()

    set(${out_latin_font} "${latin_font}" PARENT_SCOPE)
    set(${out_cjk_font} "${cjk_font}" PARENT_SCOPE)
endfunction()

function(rynui_resolve_shadercross_host_tool out_command out_dependency)
    if(RYNUI_SHADERCROSS_EXECUTABLE)
        if(NOT EXISTS "${RYNUI_SHADERCROSS_EXECUTABLE}")
            message(FATAL_ERROR
                "RYNUI_SHADERCROSS_EXECUTABLE does not exist: "
                "${RYNUI_SHADERCROSS_EXECUTABLE}")
        endif()
        if(IS_DIRECTORY "${RYNUI_SHADERCROSS_EXECUTABLE}")
            message(FATAL_ERROR
                "RYNUI_SHADERCROSS_EXECUTABLE must name a runnable host tool, not a directory: "
                "${RYNUI_SHADERCROSS_EXECUTABLE}")
        endif()

        get_filename_component(shadercross_executable
            "${RYNUI_SHADERCROSS_EXECUTABLE}" REALPATH)
        execute_process(
            COMMAND "${shadercross_executable}" --help
            RESULT_VARIABLE shadercross_probe_result
            OUTPUT_VARIABLE shadercross_probe_stdout
            ERROR_VARIABLE shadercross_probe_stderr
            TIMEOUT 10
        )
        set(shadercross_probe_output
            "${shadercross_probe_stdout}\n${shadercross_probe_stderr}")
        if(NOT shadercross_probe_result EQUAL 0
                OR NOT shadercross_probe_output MATCHES "Usage:[ \t]+shadercross")
            message(FATAL_ERROR
                "RYNUI_SHADERCROSS_EXECUTABLE is not a runnable SDL_shadercross host tool: "
                "${shadercross_executable} (probe result: ${shadercross_probe_result}).")
        endif()

        message(STATUS "Using SDL_shadercross host tool override: ${shadercross_executable}")
        set(${out_command} "${shadercross_executable}" PARENT_SCOPE)
        set(${out_dependency} "" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "Cross-compiling requires a runnable host SDL_shadercross CLI. "
            "Set RYNUI_SHADERCROSS_EXECUTABLE to the host executable; a target "
            "binary will never be executed during the build.")
    endif()

    if(RYNUI_DEPENDENCY_MODE STREQUAL "SYSTEM")
        message(FATAL_ERROR
            "RYNUI_DEPENDENCY_MODE=SYSTEM requires RYNUI_SHADERCROSS_EXECUTABLE. "
            "Provide a runnable SDL_shadercross CLI from the host environment.")
    endif()

    if(NOT RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        message(FATAL_ERROR
            "Cannot resolve SDL_shadercross for invalid RYNUI_DEPENDENCY_MODE="
            "${RYNUI_DEPENDENCY_MODE}.")
    endif()

    include(FetchContent)

    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(dxc_url "${RYNUI_DXC_WINDOWS_X64_URL}")
        set(dxc_hash "${RYNUI_DXC_WINDOWS_X64_SHA256}")
        set(dxc_include_path "${rynui_dxc_SOURCE_DIR}/inc")
        set(dxc_compiler_binary "${rynui_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
        set(dxc_compiler_library "${rynui_dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib")
        set(dxc_dxil_binary "${rynui_dxc_SOURCE_DIR}/bin/x64/dxil.dll")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux"
            AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(dxc_url "${RYNUI_DXC_LINUX_X64_URL}")
        set(dxc_hash "${RYNUI_DXC_LINUX_X64_SHA256}")
        set(dxc_include_path "${rynui_dxc_SOURCE_DIR}/include/dxc")
        set(dxc_compiler_library "${rynui_dxc_SOURCE_DIR}/lib/libdxcompiler.so")
        set(dxc_dxil_library "${rynui_dxc_SOURCE_DIR}/lib/libdxil.so")
    else()
        message(FATAL_ERROR
            "The bundled SDL_shadercross host tool is locked only for Windows x64 "
            "and Linux x86_64. Set RYNUI_SHADERCROSS_EXECUTABLE for this host.")
    endif()

    message(STATUS
        "Resolving DirectXShaderCompiler ${RYNUI_DXC_VERSION} host binaries from "
        "the locked archive.")
    FetchContent_Declare(
        rynui_dxc
        URL "${dxc_url}"
        URL_HASH "SHA256=${dxc_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        SOURCE_SUBDIR "__rynui_no_cmake_project__"
    )
    FetchContent_MakeAvailable(rynui_dxc)

    # The paths above intentionally refer to rynui_dxc_SOURCE_DIR after
    # population. Re-evaluate them now that FetchContent has assigned it.
    if(WIN32)
        set(dxc_include_path "${rynui_dxc_SOURCE_DIR}/inc")
        set(dxc_compiler_binary "${rynui_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
        set(dxc_compiler_library "${rynui_dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib")
        set(dxc_dxil_binary "${rynui_dxc_SOURCE_DIR}/bin/x64/dxil.dll")
    else()
        set(dxc_include_path "${rynui_dxc_SOURCE_DIR}/include/dxc")
        set(dxc_compiler_library "${rynui_dxc_SOURCE_DIR}/lib/libdxcompiler.so")
        set(dxc_dxil_library "${rynui_dxc_SOURCE_DIR}/lib/libdxil.so")
    endif()

    set(SPIRV_CROSS_SHARED OFF CACHE BOOL "Build shared SPIRV-Cross libraries" FORCE)
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "Build static SPIRV-Cross libraries" FORCE)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "Build the SPIRV-Cross CLI" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Build SPIRV-Cross tests" FORCE)
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "Skip SPIRV-Cross install rules" FORCE)

    message(STATUS
        "Resolving SPIRV-Cross ${RYNUI_SPIRV_CROSS_COMMIT} from the locked archive.")
    FetchContent_Declare(
        rynui_spirv_cross
        URL "${RYNUI_SPIRV_CROSS_SOURCE_URL}"
        URL_HASH "SHA256=${RYNUI_SPIRV_CROSS_SOURCE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(rynui_spirv_cross)

    if(NOT TARGET spirv-cross-c)
        message(FATAL_ERROR "Locked SPIRV-Cross source did not define spirv-cross-c.")
    endif()

    if(NOT TARGET DirectXShaderCompiler::dxcompiler)
        add_library(DirectXShaderCompiler::dxcompiler SHARED IMPORTED GLOBAL)
        set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${dxc_include_path}"
        )
    endif()
    if(WIN32)
        set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
            IMPORTED_LOCATION "${dxc_compiler_binary}"
            IMPORTED_IMPLIB "${dxc_compiler_library}"
        )
    else()
        set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
            IMPORTED_LOCATION "${dxc_compiler_library}"
        )
    endif()

    message(STATUS
        "Resolving SDL_shadercross ${RYNUI_SDL_SHADERCROSS_COMMIT} CLI sources from "
        "the locked archive.")
    FetchContent_Declare(
        rynui_sdl_shadercross
        URL "${RYNUI_SDL_SHADERCROSS_SOURCE_URL}"
        URL_HASH "SHA256=${RYNUI_SDL_SHADERCROSS_SOURCE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        SOURCE_SUBDIR "__rynui_no_cmake_project__"
    )
    FetchContent_MakeAvailable(rynui_sdl_shadercross)

    add_library(rynui_shadercross_library STATIC
        "${rynui_sdl_shadercross_SOURCE_DIR}/src/SDL_shadercross.c"
    )
    target_compile_features(rynui_shadercross_library PRIVATE c_std_99)
    target_compile_definitions(rynui_shadercross_library PRIVATE SDL_SHADERCROSS_DXC)
    target_include_directories(rynui_shadercross_library PUBLIC
        "${rynui_sdl_shadercross_SOURCE_DIR}/include"
    )
    target_link_libraries(rynui_shadercross_library PRIVATE
        SDL3::SDL3
        spirv-cross-c
        DirectXShaderCompiler::dxcompiler
    )
    set_property(TARGET rynui_shadercross_library PROPERTY LINKER_LANGUAGE CXX)

    add_executable(shadercross
        "${rynui_sdl_shadercross_SOURCE_DIR}/src/cli.c"
    )
    target_compile_features(shadercross PRIVATE c_std_99)
    target_link_libraries(shadercross PRIVATE
        rynui_shadercross_library
        SDL3::SDL3
    )

    if(WIN32)
        add_custom_target(rynui_shadercross_host_runtime
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dxc_compiler_binary}" "$<TARGET_FILE_DIR:shadercross>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dxc_dxil_binary}" "$<TARGET_FILE_DIR:shadercross>"
            DEPENDS shadercross
            VERBATIM
        )
        set(shadercross_dependency rynui_shadercross_host_runtime)
    else()
        set_property(TARGET shadercross PROPERTY BUILD_RPATH "${rynui_dxc_SOURCE_DIR}/lib")
        set(shadercross_dependency shadercross)
    endif()

    set(${out_command} "$<TARGET_FILE:shadercross>" PARENT_SCOPE)
    set(${out_dependency} "${shadercross_dependency}" PARENT_SCOPE)
endfunction()
