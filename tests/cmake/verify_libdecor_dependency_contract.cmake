cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED TEST_SOURCE_DIR)
    message(FATAL_ERROR "TEST_SOURCE_DIR is required.")
endif()

file(READ "${TEST_SOURCE_DIR}/cmake/RynUIDependencies.cmake" dependency_source)

foreach(pattern IN ITEMS
        "function\\(rynui_resolve_bundled_libdecor\\)"
        "CMAKE_SYSTEM_NAME STREQUAL \"Linux\""
        "RYNUI_DEPENDENCY_MODE STREQUAL \"BUNDLED\""
        "ExternalProject_Add\\("
        "--buildtype debugoptimized"
        "-Ddemo=false"
        "-Ddbus=disabled"
        "-Dgtk=disabled"
        "add_library\\(RynUI::LibDecor ALIAS rynui_libdecor\\)"
        "lib/libdecor/plugins-1"
        "RYNUI_LIBDECOR_BUILD_RPATH"
        "libdecor_preconfigure_patch_result"
        "BUILD_BYPRODUCTS")
    if(NOT dependency_source MATCHES "${pattern}")
        message(FATAL_ERROR "Bundled libdecor dependency contract is missing: ${pattern}")
    endif()
endforeach()

if(dependency_source MATCHES "RYNUI_DEPENDENCY_MODE STREQUAL \"SYSTEM\"[^\n]*rynui_resolve_bundled_libdecor")
    message(FATAL_ERROR "SYSTEM mode must not resolve the bundled libdecor source.")
endif()

message(STATUS "libdecor remains a Linux-only explicit BUNDLED dependency")
