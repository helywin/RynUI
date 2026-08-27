cmake_minimum_required(VERSION 3.25)

project(RynUIPropReferenceContract LANGUAGES CXX)

if(NOT DEFINED RYNUI_ROOT OR "${RYNUI_ROOT}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_ROOT is required.")
endif()

add_library(prop_reference_compile_fail STATIC prop_reference.cpp)
target_compile_features(prop_reference_compile_fail PRIVATE cxx_std_20)
set_target_properties(prop_reference_compile_fail PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    prop_reference_compile_fail
    PRIVATE "${RYNUI_ROOT}/include"
)
