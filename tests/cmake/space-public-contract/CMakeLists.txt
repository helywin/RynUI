cmake_minimum_required(VERSION 3.25)
project(RynUISpacePublicContract LANGUAGES CXX)

if(NOT DEFINED RYNUI_ROOT OR NOT DEFINED SPACE_CONTRACT_CASE)
    message(FATAL_ERROR "RYNUI_ROOT and SPACE_CONTRACT_CASE are required")
endif()

if(SPACE_CONTRACT_CASE STREQUAL "string-values")
    set(contract_source space_string_values.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "internal-enum")
    set(contract_source space_internal_enum.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "foreign-slot")
    set(contract_source space_foreign_slot.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "visual-entry")
    set(contract_source space_visual_entry.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "baseline")
    set(contract_source space_baseline.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "separator")
    set(contract_source space_separator.cpp)
elseif(SPACE_CONTRACT_CASE STREQUAL "compact")
    set(contract_source space_compact.cpp)
else()
    message(FATAL_ERROR "Unknown SPACE_CONTRACT_CASE=${SPACE_CONTRACT_CASE}")
endif()

add_library(space_public_compile_fail STATIC "${contract_source}")
target_compile_features(space_public_compile_fail PRIVATE cxx_std_20)
set_target_properties(space_public_compile_fail PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    space_public_compile_fail
    PRIVATE "${RYNUI_ROOT}/include" "${RYNUI_ROOT}/src"
)
