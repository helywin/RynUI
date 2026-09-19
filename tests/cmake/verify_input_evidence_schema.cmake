cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR "${EVIDENCE}" STREQUAL "" OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "Input evidence report is required")
endif()
if(NOT DEFINED EXPECTED_SCOPE
        OR NOT EXPECTED_SCOPE MATCHES "^(platform-generic|windows|linux)$")
    message(FATAL_ERROR "EXPECTED_SCOPE must be platform-generic, windows, or linux")
endif()

file(READ "${EVIDENCE}" report)
if(report MATCHES "status=planning-only|result=planned|implemented=false")
    message(FATAL_ERROR "Planning-only text cannot pass Input evidence")
endif()

set(required_keys
    schema_version
    change
    scope
    platform
    status
    execution_platform
    os
    compiler
    preset
    build_system
    cpp_standard
    utf8proc_version
    unicode_version
    unicode_fixture_sha256
    api_header
    input_modes
    value_selection
    composition_history
    clipboard_session_input_area
    input_token_source
    scene_topology
    upload_diagnostics
    frame_diagnostics
    input_area_scales
    allocation_count
    frame_submissions
    idle_waits
    idle_restored
    dependency_mode
    window_system
    driver
    shader_format
    font_source
    ime_path
    manual_confirmation_path
    unit_tests_exit_code
    headless_tests_exit_code
    contract_tests_exit_code
    benchmark_tests_exit_code
    commit_sha
    git_diff_check_exit_code
    exit_code)

foreach(key IN LISTS required_keys)
    file(STRINGS "${EVIDENCE}" line REGEX "^${key}=.+$")
    if("${line}" STREQUAL "")
        message(FATAL_ERROR "Input evidence is missing: ${key}")
    endif()
endforeach()

file(STRINGS "${EVIDENCE}" scope_line REGEX "^scope=${EXPECTED_SCOPE}$")
file(STRINGS "${EVIDENCE}" platform_line REGEX "^platform=${EXPECTED_SCOPE}$")
if("${scope_line}" STREQUAL "" OR "${platform_line}" STREQUAL "")
    message(FATAL_ERROR "Input evidence scope does not match ${EXPECTED_SCOPE}")
endif()

file(STRINGS "${EVIDENCE}" status_line REGEX "^status=.+$")
if(EXPECTED_SCOPE STREQUAL "windows")
    foreach(identity IN ITEMS
            "execution_platform=windows"
            "preset=windows-msvc"
            "window_system=win32")
        string(FIND "${report}" "${identity}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Windows Input evidence has a cross-platform identity: ${identity}")
        endif()
    endforeach()
    if(status_line STREQUAL "status=passed" AND NOT report MATCHES "compiler=MSVC")
        message(FATAL_ERROR "Passed Windows Input evidence must use MSVC")
    endif()
elseif(EXPECTED_SCOPE STREQUAL "linux")
    foreach(identity IN ITEMS
            "execution_platform=linux"
            "preset=linux-"
            "window_system=wayland")
        string(FIND "${report}" "${identity}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Linux Input evidence has a cross-platform identity: ${identity}")
        endif()
    endforeach()
    if(status_line STREQUAL "status=passed" AND NOT report MATCHES "compiler=(GCC|Clang)")
        message(FATAL_ERROR "Passed Linux Input evidence must use GCC or Clang")
    endif()
else()
    file(STRINGS "${EVIDENCE}" execution_line REGEX "^execution_platform=(windows|linux)$")
    if("${execution_line}" STREQUAL "")
        message(FATAL_ERROR "Platform-generic Input evidence needs its actual execution platform")
    endif()
    if(status_line STREQUAL "status=passed")
        if(execution_line STREQUAL "execution_platform=windows")
            if(NOT report MATCHES "compiler=MSVC" OR NOT report MATCHES "preset=windows-msvc")
                message(FATAL_ERROR "Generic Input execution identity mixes Windows with another toolchain")
            endif()
        elseif(NOT report MATCHES "compiler=(GCC|Clang)" OR NOT report MATCHES "preset=linux-")
            message(FATAL_ERROR "Generic Input execution identity mixes Linux with another toolchain")
        endif()
    endif()
endif()

if(REQUIRE_PASSED)
    file(STRINGS "${EVIDENCE}" passed REGEX "^status=passed$")
    string(FIND "${report}" "=pending" pending)
    if("${passed}" STREQUAL "" OR NOT pending EQUAL -1)
        message(FATAL_ERROR "Input evidence is not complete")
    endif()
    foreach(counter IN ITEMS allocation_count frame_submissions idle_waits)
        file(STRINGS "${EVIDENCE}" numeric REGEX "^${counter}=[0-9]+$")
        if("${numeric}" STREQUAL "")
            message(FATAL_ERROR "Input evidence counter is not numeric: ${counter}")
        endif()
    endforeach()
    foreach(exact IN ITEMS
            "schema_version=1"
            "change=010-20260831-build-text-input-foundation"
            "build_system=Ninja Multi-Config"
            "cpp_standard=C++20"
            "utf8proc_version=2.11.3"
            "unicode_version=17.0.0"
            "api_header=include/ryn/input.hpp"
            "input_modes=controlled,uncontrolled"
            "input_area_scales=1.0,1.25,1.5,2.0"
            "allocation_count=0"
            "idle_restored=true"
            "dependency_mode=BUNDLED"
            "unit_tests_exit_code=0"
            "headless_tests_exit_code=0"
            "contract_tests_exit_code=0"
            "benchmark_tests_exit_code=0"
            "git_diff_check_exit_code=0"
            "exit_code=0")
        string(FIND "${report}" "${exact}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Passed Input evidence is missing: ${exact}")
        endif()
    endforeach()
    file(STRINGS "${EVIDENCE}" commit_line REGEX "^commit_sha=[0-9a-f]+$")
    string(REGEX REPLACE "^commit_sha=" "" commit_value "${commit_line}")
    string(LENGTH "${commit_value}" commit_length)
    if("${commit_line}" STREQUAL "" OR NOT commit_length EQUAL 40)
        message(FATAL_ERROR "Passed Input evidence requires a full Git commit SHA")
    endif()
    if(EXPECTED_SCOPE STREQUAL "platform-generic")
        file(STRINGS "${EVIDENCE}" manual_path
            REGEX "^manual_confirmation_path=not-required-platform-generic$")
        if("${manual_path}" STREQUAL "")
            message(FATAL_ERROR "Platform-generic Input evidence cannot claim manual platform acceptance")
        endif()
    else()
        file(STRINGS "${EVIDENCE}" manual_path
            REGEX "^manual_confirmation_path=evidence/${EXPECTED_SCOPE}-input-.+$")
        if("${manual_path}" STREQUAL "")
            message(FATAL_ERROR "Platform Input manual confirmation path is not scope-specific")
        endif()
        string(REGEX REPLACE "^manual_confirmation_path=" "" manual_relative "${manual_path}")
        get_filename_component(evidence_directory "${EVIDENCE}" DIRECTORY)
        get_filename_component(change_directory "${evidence_directory}" DIRECTORY)
        if(NOT EXISTS "${change_directory}/${manual_relative}")
            message(FATAL_ERROR "Platform Input manual confirmation evidence is missing")
        endif()
    endif()
endif()
