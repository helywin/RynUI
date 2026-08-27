cmake_minimum_required(VERSION 3.25)
project(RynUITextPublicContract LANGUAGES CXX)

if(NOT DEFINED RYNUI_ROOT OR NOT DEFINED TEXT_CONTRACT_CASE)
    message(FATAL_ERROR "RYNUI_ROOT and TEXT_CONTRACT_CASE are required")
endif()

if(TEXT_CONTRACT_CASE STREQUAL "narrow-literal")
    set(contract_source text_narrow_literal.cpp)
elseif(TEXT_CONTRACT_CASE STREQUAL "borrowed-view")
    set(contract_source text_borrowed_view.cpp)
else()
    message(FATAL_ERROR "Unknown TEXT_CONTRACT_CASE=${TEXT_CONTRACT_CASE}")
endif()

add_library(text_public_compile_fail STATIC "${contract_source}")
target_compile_features(text_public_compile_fail PRIVATE cxx_std_20)
set_target_properties(text_public_compile_fail PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(text_public_compile_fail PRIVATE "${RYNUI_ROOT}/include")
