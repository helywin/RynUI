cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR "${EVIDENCE}" STREQUAL "" OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "Animation evidence report is required")
endif()
if(NOT DEFINED EXPECTED_SCOPE
        OR NOT EXPECTED_SCOPE MATCHES "^(platform-generic|windows|linux)$")
    message(FATAL_ERROR "EXPECTED_SCOPE must be platform-generic, windows, or linux")
endif()

file(READ "${EVIDENCE}" report)
string(FIND "${report}" "status=planning-only" planning_only)
if(NOT planning_only EQUAL -1)
    message(FATAL_ERROR "Planning-only text cannot pass animation evidence")
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
    clock_source
    clock_resolution
    cadence_hz
    theme_source
    motion_tokens
    motion_preference
    lifecycle_created
    lifecycle_completed
    lifecycle_canceled
    lifecycle_retargeted
    lifecycle_active
    allocation_count
    button_journey
    spinner_segments
    spinner_phase_wrap
    target_dirty_domains
    target_material_range_count
    gpu_deferred_retries
    frame_submissions
    animation_frames
    deferred_submissions
    idle_waits
    idle_after_animation
    last_animation_idle
    dependency_mode
    driver
    shader_format
    font_source
    real_window_evidence_path
    unit_tests_exit_code
    headless_tests_exit_code
    contract_tests_exit_code
    benchmark_tests_exit_code
    openspec_validate_exit_code
    openspec_doctor_healthy
    git_diff_check_exit_code
    exit_code)

foreach(key IN LISTS required_keys)
    file(STRINGS "${EVIDENCE}" line REGEX "^${key}=.+$")
    if("${line}" STREQUAL "")
        message(FATAL_ERROR "Animation evidence is missing: ${key}")
    endif()
endforeach()

file(STRINGS "${EVIDENCE}" scope_line REGEX "^scope=${EXPECTED_SCOPE}$")
file(STRINGS "${EVIDENCE}" platform_line REGEX "^platform=${EXPECTED_SCOPE}$")
if("${scope_line}" STREQUAL "" OR "${platform_line}" STREQUAL "")
    message(FATAL_ERROR
        "Animation evidence scope does not match ${EXPECTED_SCOPE}")
endif()
if(EXPECTED_SCOPE STREQUAL "windows")
    string(FIND "${report}" "platform=linux" other_platform)
elseif(EXPECTED_SCOPE STREQUAL "linux")
    string(FIND "${report}" "platform=windows" other_platform)
else()
    set(other_platform -1)
endif()
if(NOT other_platform EQUAL -1)
    message(FATAL_ERROR "Animation evidence contains a cross-platform identity")
endif()

if(REQUIRE_PASSED)
    file(STRINGS "${EVIDENCE}" passed REGEX "^status=passed$")
    string(FIND "${report}" "=pending" pending)
    if("${passed}" STREQUAL "" OR NOT pending EQUAL -1)
        message(FATAL_ERROR "Animation evidence is not complete")
    endif()
    foreach(counter IN ITEMS
            lifecycle_created lifecycle_completed lifecycle_canceled
            lifecycle_retargeted lifecycle_active allocation_count
            target_material_range_count gpu_deferred_retries frame_submissions
            animation_frames deferred_submissions idle_waits idle_after_animation)
        file(STRINGS "${EVIDENCE}" numeric REGEX "^${counter}=[0-9]+$")
        if("${numeric}" STREQUAL "")
            message(FATAL_ERROR "Animation evidence counter is not numeric: ${counter}")
        endif()
    endforeach()
    foreach(exact IN ITEMS
            "schema_version=1"
            "change=009-20260829-build-animation-runtime-foundation"
            "build_system=Ninja Multi-Config"
            "cpp_standard=C++20"
            "clock_resolution=integer-microseconds"
            "cadence_hz=60,120,144"
            "lifecycle_active=0"
            "allocation_count=0"
            "spinner_segments=8"
            "spinner_phase_wrap=passed"
            "target_dirty_domains=Material,Animation"
            "last_animation_idle=true"
            "unit_tests_exit_code=0"
            "headless_tests_exit_code=0"
            "contract_tests_exit_code=0"
            "benchmark_tests_exit_code=0"
            "openspec_validate_exit_code=0"
            "openspec_doctor_healthy=true"
            "git_diff_check_exit_code=0"
            "exit_code=0")
        string(FIND "${report}" "${exact}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Animation passed evidence is missing: ${exact}")
        endif()
    endforeach()
    if(EXPECTED_SCOPE STREQUAL "platform-generic")
        file(STRINGS
            "${EVIDENCE}"
            generic_window_path
            REGEX "^real_window_evidence_path=not-required-platform-generic$")
        if("${generic_window_path}" STREQUAL "")
            message(FATAL_ERROR
                "Platform-generic animation evidence cannot claim real-window acceptance")
        endif()
    else()
        file(STRINGS
            "${EVIDENCE}"
            window_path_line
            REGEX "^real_window_evidence_path=evidence/${EXPECTED_SCOPE}-animation-.+$")
        if("${window_path_line}" STREQUAL "")
            message(FATAL_ERROR
                "Platform animation evidence path is not platform-specific")
        endif()
        string(REGEX REPLACE
            "^real_window_evidence_path=" "" window_relative "${window_path_line}")
        get_filename_component(evidence_directory "${EVIDENCE}" DIRECTORY)
        get_filename_component(change_directory "${evidence_directory}" DIRECTORY)
        if(NOT EXISTS "${change_directory}/${window_relative}")
            message(FATAL_ERROR "Platform real-window animation evidence is missing")
        endif()
    endif()
endif()
