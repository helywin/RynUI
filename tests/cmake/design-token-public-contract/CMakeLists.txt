cmake_minimum_required(VERSION 3.25)
project(RynUIDesignTokenPublicContract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DESIGN_TOKEN_CONTRACT_CASE STREQUAL "css-string")
    set(contract_source design_token_css_string.cpp)
elseif(DESIGN_TOKEN_CONTRACT_CASE STREQUAL "arbitrary-key")
    set(contract_source design_token_arbitrary_key.cpp)
elseif(DESIGN_TOKEN_CONTRACT_CASE STREQUAL "invalid-constexpr")
    set(contract_source design_token_invalid_constexpr.cpp)
else()
    message(FATAL_ERROR "Unknown DESIGN_TOKEN_CONTRACT_CASE")
endif()

add_executable(contract "${contract_source}")
target_include_directories(contract PRIVATE "${RYNUI_ROOT}/include")
