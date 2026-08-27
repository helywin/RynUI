cmake_minimum_required(VERSION 3.25)
project(RynUIFlexPublicContract LANGUAGES CXX)

if(NOT DEFINED RYNUI_ROOT OR NOT DEFINED FLEX_CONTRACT_CASE)
    message(FATAL_ERROR "RYNUI_ROOT and FLEX_CONTRACT_CASE are required")
endif()

if(FLEX_CONTRACT_CASE STREQUAL "string-values")
    set(contract_source flex_string_values.cpp)
elseif(FLEX_CONTRACT_CASE STREQUAL "internal-enum")
    set(contract_source flex_internal_enum.cpp)
elseif(FLEX_CONTRACT_CASE STREQUAL "foreign-slot")
    set(contract_source flex_foreign_slot.cpp)
elseif(FLEX_CONTRACT_CASE STREQUAL "visual-entry")
    set(contract_source flex_visual_entry.cpp)
else()
    message(FATAL_ERROR "Unknown FLEX_CONTRACT_CASE=${FLEX_CONTRACT_CASE}")
endif()

add_library(flex_public_compile_fail STATIC "${contract_source}")
target_compile_features(flex_public_compile_fail PRIVATE cxx_std_20)
set_target_properties(flex_public_compile_fail PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    flex_public_compile_fail
    PRIVATE "${RYNUI_ROOT}/include" "${RYNUI_ROOT}/src"
)
