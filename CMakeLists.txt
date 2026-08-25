cmake_minimum_required(VERSION 3.25)

project(RynUI VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(RynUIArchitecture)
include(RynUIDependencies)
include(CTest)

option(RYNUI_BUILD_EXAMPLES "Build RynUI examples" ON)
set(RYNUI_EXPECTED_TOOLCHAIN "" CACHE STRING "Expected compiler family for preset validation")
set_property(CACHE RYNUI_EXPECTED_TOOLCHAIN PROPERTY STRINGS "" msvc gcc clang)

if(PROJECT_IS_TOP_LEVEL)
    rynui_verify_build_contract()
endif()

rynui_resolve_sdl3()

add_subdirectory(src)

rynui_verify_public_api("${CMAKE_CURRENT_SOURCE_DIR}/include/ryn")

if(RYNUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
