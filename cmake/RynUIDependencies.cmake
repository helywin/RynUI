include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/dependencies/RynUIDependencyLock.cmake")

set(RYNUI_DEPENDENCY_MODE "BUNDLED" CACHE STRING
    "Third-party dependency source: BUNDLED or SYSTEM")
set_property(CACHE RYNUI_DEPENDENCY_MODE PROPERTY STRINGS BUNDLED SYSTEM)

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
