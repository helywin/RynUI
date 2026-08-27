cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS LINUX_EVIDENCE WINDOWS_EVIDENCE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
    if(NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} report is missing")
    endif()
endforeach()

file(REAL_PATH "${LINUX_EVIDENCE}" linux_evidence_path)
file(REAL_PATH "${WINDOWS_EVIDENCE}" windows_evidence_path)
if(linux_evidence_path STREQUAL windows_evidence_path)
    message(FATAL_ERROR "Linux and Windows Button evidence cannot share one file")
endif()

set(required_keys
    schema_version
    change
    platform
    status
    preset_debug
    preset_release
    build_system
    compiler
    display_scale
    gpu_driver
    shader_format
    exit_code
    screenshot_path
    input_events
    hit_test_queries
    routes_dispatched
    captures_started
    captures_released
    focus_changes
    clicks
    prop_updates
    layout_passes
    scene_rebuilds
    quad_uploads
    quad_uploaded_bytes
    glyph_uploads
    glyph_uploaded_bytes
    quad_draws
    glyph_draws
    frame_submissions
    idle_waits)

function(validate_evidence report_path expected_platform other_platform)
    file(READ "${report_path}" report)
    foreach(key IN LISTS required_keys)
        string(FIND "${report}" "${key}=" key_found)
        if(key_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} Button evidence is missing field: ${key}")
        endif()
    endforeach()
    string(FIND "${report}" "platform=${expected_platform}" expected_found)
    string(FIND "${report}" "platform=${other_platform}" other_found)
    if(expected_found EQUAL -1 OR NOT other_found EQUAL -1)
        message(FATAL_ERROR
            "${expected_platform} Button evidence has a cross-platform identity")
    endif()
    string(FIND
        "${report}"
        "screenshot_path=evidence/${expected_platform}-button-"
        screenshot_found)
    if(screenshot_found EQUAL -1)
        message(FATAL_ERROR
            "${expected_platform} Button screenshot path is not platform-specific")
    endif()
    set(require_pass FALSE)
    if(DEFINED REQUIRE_PASSED_PLATFORM)
        if(REQUIRE_PASSED_PLATFORM STREQUAL "both"
                OR REQUIRE_PASSED_PLATFORM STREQUAL "${expected_platform}")
            set(require_pass TRUE)
        elseif(NOT REQUIRE_PASSED_PLATFORM STREQUAL "linux"
                AND NOT REQUIRE_PASSED_PLATFORM STREQUAL "windows")
            message(FATAL_ERROR
                "REQUIRE_PASSED_PLATFORM must be linux, windows, or both")
        endif()
    endif()
    if(require_pass)
        string(FIND "${report}" "status=passed" passed_found)
        string(FIND "${report}" "=pending" pending_found)
        if(passed_found EQUAL -1 OR NOT pending_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} Button evidence is not complete")
        endif()
        foreach(counter IN ITEMS
                input_events
                hit_test_queries
                routes_dispatched
                captures_started
                captures_released
                focus_changes
                clicks
                prop_updates
                layout_passes
                scene_rebuilds
                quad_uploads
                quad_uploaded_bytes
                glyph_uploads
                glyph_uploaded_bytes
                quad_draws
                glyph_draws
                frame_submissions
                idle_waits)
            file(STRINGS
                "${report_path}"
                counter_line
                REGEX "^${counter}=[0-9]+$")
            if("${counter_line}" STREQUAL "")
                message(FATAL_ERROR
                    "${expected_platform} Button counter is not numeric: ${counter}")
            endif()
        endforeach()
        file(STRINGS
            "${report_path}"
            successful_exit
            REGEX "^exit_code=0$")
        if("${successful_exit}" STREQUAL "")
            message(FATAL_ERROR
                "${expected_platform} Button evidence has no successful exit")
        endif()
        file(STRINGS
            "${report_path}"
            screenshot_line
            REGEX "^screenshot_path=.+$")
        string(REGEX REPLACE "^screenshot_path=" "" screenshot_relative
            "${screenshot_line}")
        get_filename_component(evidence_directory "${report_path}" DIRECTORY)
        get_filename_component(change_directory "${evidence_directory}" DIRECTORY)
        set(screenshot "${change_directory}/${screenshot_relative}")
        if(NOT EXISTS "${screenshot}")
            message(FATAL_ERROR
                "${expected_platform} Button evidence screenshot is missing")
        endif()
        file(SIZE "${screenshot}" screenshot_size)
        if(screenshot_size LESS 4096)
            message(FATAL_ERROR
                "${expected_platform} Button evidence screenshot is too small")
        endif()
    endif()
endfunction()

validate_evidence("${linux_evidence_path}" linux windows)
validate_evidence("${windows_evidence_path}" windows linux)
