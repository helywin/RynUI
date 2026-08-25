cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR)
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required.")
endif()

include("${RYNUI_SOURCE_DIR}/cmake/dependencies/RynUIDependencyLock.cmake")

set(required_variables
    RYNUI_SDL3_VERSION
    RYNUI_SDL3_COMMIT
    RYNUI_SDL3_SOURCE_URL
    RYNUI_SDL3_SOURCE_SHA256
    RYNUI_SDL3_LICENSE
    RYNUI_SDL_SHADERCROSS_COMMIT
    RYNUI_SDL_SHADERCROSS_SOURCE_URL
    RYNUI_SDL_SHADERCROSS_SOURCE_SHA256
    RYNUI_SDL_SHADERCROSS_LICENSE
)

foreach(required_variable IN LISTS required_variables)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Dependency lock is missing ${required_variable}.")
    endif()
endforeach()

foreach(hash_variable IN ITEMS
        RYNUI_SDL3_SOURCE_SHA256
        RYNUI_SDL_SHADERCROSS_SOURCE_SHA256)
    string(LENGTH "${${hash_variable}}" hash_length)
    if(NOT hash_length EQUAL 64 OR NOT "${${hash_variable}}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${hash_variable} must be a lowercase SHA256 value.")
    endif()
endforeach()

if(NOT RYNUI_SDL3_SOURCE_URL MATCHES "3[.]4[.]14"
        OR RYNUI_SDL3_SOURCE_URL MATCHES "latest|refs/heads")
    message(FATAL_ERROR "SDL3 source URL is not tied to the locked release.")
endif()

if(NOT RYNUI_SDL_SHADERCROSS_SOURCE_URL MATCHES "${RYNUI_SDL_SHADERCROSS_COMMIT}"
        OR RYNUI_SDL_SHADERCROSS_SOURCE_URL MATCHES "latest|refs/heads")
    message(FATAL_ERROR "SDL_shadercross source URL is not tied to its locked commit.")
endif()
