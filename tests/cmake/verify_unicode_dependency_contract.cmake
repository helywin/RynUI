cmake_minimum_required(VERSION 3.25)
include("${TEST_SOURCE_DIR}/cmake/dependencies/RynUIDependencyLock.cmake")
if(NOT RYNUI_UTF8PROC_VERSION STREQUAL "2.11.3"
        OR NOT RYNUI_UTF8PROC_LICENSE STREQUAL "MIT AND Unicode-DFS-2015"
        OR NOT RYNUI_GRAPHEME_TEST_LICENSE STREQUAL "Unicode-3.0"
        OR NOT RYNUI_UTF8PROC_SOURCE_URL MATCHES "/v2[.]11[.]3/utf8proc-2[.]11[.]3[.]tar[.]gz$"
        OR NOT RYNUI_UNICODE_VERSION STREQUAL "17.0.0"
        OR NOT RYNUI_GRAPHEME_TEST_SOURCE_URL MATCHES "/17[.]0[.]0/ucd/auxiliary/GraphemeBreakTest[.]txt$")
    message(FATAL_ERROR "Unicode dependency identity drift")
endif()
file(GLOB_RECURSE headers "${TEST_SOURCE_DIR}/include/ryn/*.hpp")
list(APPEND headers "${TEST_SOURCE_DIR}/src/input/text_boundary.hpp")
foreach(header IN LISTS headers)
    file(READ "${header}" contents)
    if(contents MATCHES "#[ \t]*include[^\n]*(utf8proc|SDL)")
        message(FATAL_ERROR "Unicode/platform header leaked: ${header}")
    endif()
endforeach()
file(READ "${TEST_SOURCE_DIR}/cmake/dependencies/RynUIUtf8proc.cmake" resolver)
if(resolver MATCHES "GIT_REPOSITORY|QUIET|find_library|find_path")
    message(FATAL_ERROR "Unicode resolver must not use implicit fallback")
endif()
file(READ "${TEST_SOURCE_DIR}/third_party/licenses/utf8proc-2.11.3.txt" license)
if(NOT license MATCHES "MIT AND Unicode-DFS-2015" OR NOT license MATCHES "LICENSE.md")
    message(FATAL_ERROR "Missing Unicode license record")
endif()
