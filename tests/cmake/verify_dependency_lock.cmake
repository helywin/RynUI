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
    RYNUI_SPIRV_CROSS_COMMIT
    RYNUI_SPIRV_CROSS_SOURCE_URL
    RYNUI_SPIRV_CROSS_SOURCE_SHA256
    RYNUI_SPIRV_CROSS_LICENSE
    RYNUI_DXC_VERSION
    RYNUI_DXC_WINDOWS_X64_URL
    RYNUI_DXC_WINDOWS_X64_SHA256
    RYNUI_DXC_LINUX_X64_URL
    RYNUI_DXC_LINUX_X64_SHA256
    RYNUI_DXC_LICENSE
    RYNUI_FREETYPE_VERSION
    RYNUI_FREETYPE_COMMIT
    RYNUI_FREETYPE_SOURCE_URL
    RYNUI_FREETYPE_SOURCE_SHA256
    RYNUI_FREETYPE_LICENSE
    RYNUI_HARFBUZZ_VERSION
    RYNUI_HARFBUZZ_COMMIT
    RYNUI_HARFBUZZ_SOURCE_URL
    RYNUI_HARFBUZZ_SOURCE_SHA256
    RYNUI_HARFBUZZ_LICENSE
    RYNUI_NOTO_SANS_VERSION
    RYNUI_NOTO_SANS_COMMIT
    RYNUI_NOTO_SANS_SOURCE_URL
    RYNUI_NOTO_SANS_SOURCE_SHA256
    RYNUI_NOTO_SANS_LICENSE
    RYNUI_NOTO_SANS_CJK_SC_VERSION
    RYNUI_NOTO_SANS_CJK_SC_COMMIT
    RYNUI_NOTO_SANS_CJK_SC_SOURCE_URL
    RYNUI_NOTO_SANS_CJK_SC_SOURCE_SHA256
    RYNUI_NOTO_SANS_CJK_SC_LICENSE
)

foreach(required_variable IN LISTS required_variables)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Dependency lock is missing ${required_variable}.")
    endif()
endforeach()

foreach(hash_variable IN ITEMS
        RYNUI_SDL3_SOURCE_SHA256
        RYNUI_SDL_SHADERCROSS_SOURCE_SHA256
        RYNUI_SPIRV_CROSS_SOURCE_SHA256
        RYNUI_DXC_WINDOWS_X64_SHA256
        RYNUI_DXC_LINUX_X64_SHA256
        RYNUI_FREETYPE_SOURCE_SHA256
        RYNUI_HARFBUZZ_SOURCE_SHA256
        RYNUI_NOTO_SANS_SOURCE_SHA256
        RYNUI_NOTO_SANS_CJK_SC_SOURCE_SHA256)
    string(LENGTH "${${hash_variable}}" hash_length)
    if(NOT hash_length EQUAL 64 OR NOT "${${hash_variable}}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${hash_variable} must be a lowercase SHA256 value.")
    endif()
endforeach()

if(NOT RYNUI_SDL3_SOURCE_URL MATCHES "3[.]4[.]14"
        OR RYNUI_SDL3_SOURCE_URL MATCHES "latest|refs/heads")
    message(FATAL_ERROR "SDL3 source URL is not tied to the locked release.")
endif()

if(NOT RYNUI_SPIRV_CROSS_SOURCE_URL MATCHES "${RYNUI_SPIRV_CROSS_COMMIT}"
        OR RYNUI_SPIRV_CROSS_SOURCE_URL MATCHES "latest|refs/heads")
    message(FATAL_ERROR "SPIRV-Cross source URL is not tied to its locked commit.")
endif()

foreach(dxc_url IN ITEMS RYNUI_DXC_WINDOWS_X64_URL RYNUI_DXC_LINUX_X64_URL)
    if(NOT "${${dxc_url}}" MATCHES "v1[.]8[.]2502"
            OR "${${dxc_url}}" MATCHES "latest|refs/heads")
        message(FATAL_ERROR "${dxc_url} is not tied to the locked DXC release.")
    endif()
endforeach()

if(NOT RYNUI_SDL_SHADERCROSS_SOURCE_URL MATCHES "${RYNUI_SDL_SHADERCROSS_COMMIT}"
        OR RYNUI_SDL_SHADERCROSS_SOURCE_URL MATCHES "latest|refs/heads")
    message(FATAL_ERROR "SDL_shadercross source URL is not tied to its locked commit.")
endif()

if(NOT RYNUI_FREETYPE_SOURCE_URL MATCHES "${RYNUI_FREETYPE_VERSION}"
        OR RYNUI_FREETYPE_SOURCE_URL MATCHES "latest|refs/heads|/main/")
    message(FATAL_ERROR "FreeType source URL is not tied to its locked release.")
endif()

if(NOT RYNUI_HARFBUZZ_SOURCE_URL MATCHES "${RYNUI_HARFBUZZ_VERSION}"
        OR RYNUI_HARFBUZZ_SOURCE_URL MATCHES "latest|refs/heads|/main/")
    message(FATAL_ERROR "HarfBuzz source URL is not tied to its locked release.")
endif()

foreach(font_prefix IN ITEMS RYNUI_NOTO_SANS RYNUI_NOTO_SANS_CJK_SC)
    if(NOT "${${font_prefix}_SOURCE_URL}" MATCHES "${${font_prefix}_COMMIT}"
            OR "${${font_prefix}_SOURCE_URL}" MATCHES "latest|refs/heads|/main/")
        message(FATAL_ERROR "${font_prefix} source URL is not tied to its locked commit.")
    endif()
endforeach()

set(license_records
    "${RYNUI_SOURCE_DIR}/third_party/licenses/FreeType-2.14.3.txt"
    "${RYNUI_SOURCE_DIR}/third_party/licenses/HarfBuzz-14.3.1.txt"
    "${RYNUI_SOURCE_DIR}/third_party/licenses/Noto-validation-fonts.txt"
)
foreach(license_record IN LISTS license_records)
    if(NOT EXISTS "${license_record}")
        message(FATAL_ERROR "Missing dependency license record: ${license_record}")
    endif()
    file(READ "${license_record}" license_text)
    if(NOT license_text MATCHES "License")
        message(FATAL_ERROR "Dependency license record has no license field: ${license_record}")
    endif()
endforeach()

if(NOT DEFINED TEST_GIT_EXECUTABLE OR "${TEST_GIT_EXECUTABLE}" STREQUAL "")
    message(FATAL_ERROR "TEST_GIT_EXECUTABLE is required for the tracked-font scan.")
endif()
execute_process(
    COMMAND "${TEST_GIT_EXECUTABLE}" -C "${RYNUI_SOURCE_DIR}" ls-files --
        "*.ttf" "*.otf" "*.ttc" "*.otc" "*.woff" "*.woff2"
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE tracked_font_files
    ERROR_VARIABLE git_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT git_result EQUAL 0)
    message(FATAL_ERROR "Unable to scan tracked font files: ${git_error}")
endif()
if(NOT tracked_font_files STREQUAL "")
    message(FATAL_ERROR "Font binaries must not be tracked by Git:\n${tracked_font_files}")
endif()
