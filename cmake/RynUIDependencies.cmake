include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/dependencies/RynUIDependencyLock.cmake")

set(RYNUI_DEPENDENCY_MODE "BUNDLED" CACHE STRING
    "Third-party dependency source: BUNDLED or SYSTEM")
set_property(CACHE RYNUI_DEPENDENCY_MODE PROPERTY STRINGS BUNDLED SYSTEM)
set(RYNUI_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH
    "Runnable host SDL_shadercross CLI; required for SYSTEM and cross builds")

function(rynui_resolve_sdl3)
    set(valid_modes BUNDLED SYSTEM)
    if(NOT RYNUI_DEPENDENCY_MODE IN_LIST valid_modes)
        message(FATAL_ERROR
            "RYNUI_DEPENDENCY_MODE must be one of [BUNDLED, SYSTEM], got "
            "'${RYNUI_DEPENDENCY_MODE}'. Select BUNDLED for the locked archive "
            "or SYSTEM and provide an SDL3 CMake package.")
    endif()

    if(RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
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
