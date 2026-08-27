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
    message(FATAL_ERROR "Linux and Windows layout evidence cannot share one file")
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
    cpp_standard
    dependency_mode
    window_system
    display_scale
    pixel_density
    window_size_wide
    window_size_narrow
    pixel_size_wide
    pixel_size_narrow
    viewport_wide
    viewport_narrow
    dpi_scale_applied
    font_logical_pixel_size
    font_raster_pixel_size
    font_raster_scale
    font_source
    font_families
    gpu_driver
    shader_format
    exit_code
    screenshot_wide_path
    screenshot_narrow_path
    line_count_wide
    line_count_narrow
    content_runs
    prop_updates
    component_count
    layout_passes
    scene_rebuilds
    frame_submissions
    idle_waits)

function(validate_evidence report_path expected_platform other_platform)
    file(READ "${report_path}" report)
    foreach(key IN LISTS required_keys)
        string(FIND "${report}" "${key}=" key_found)
        if(key_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} layout evidence is missing field: ${key}")
        endif()
    endforeach()

    string(FIND "${report}" "platform=${expected_platform}" expected_found)
    string(FIND "${report}" "platform=${other_platform}" other_found)
    if(expected_found EQUAL -1 OR NOT other_found EQUAL -1)
        message(FATAL_ERROR
            "${expected_platform} layout evidence has a cross-platform identity")
    endif()

    foreach(kind IN ITEMS wide narrow)
        string(FIND
            "${report}"
            "screenshot_${kind}_path=evidence/${expected_platform}-layout-${kind}-"
            screenshot_found)
        if(screenshot_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} ${kind} screenshot path is not platform-specific")
        endif()
    endforeach()

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
                "${expected_platform} layout evidence is not complete")
        endif()
        foreach(counter IN ITEMS
                line_count_wide
                line_count_narrow
                content_runs
                prop_updates
                component_count
                layout_passes
                scene_rebuilds
                frame_submissions
                idle_waits)
            file(STRINGS
                "${report_path}"
                counter_line
                REGEX "^${counter}=[0-9]+$")
            if("${counter_line}" STREQUAL "")
                message(FATAL_ERROR
                    "${expected_platform} layout counter is not numeric: ${counter}")
            endif()
        endforeach()
        file(STRINGS "${report_path}" successful_exit REGEX "^exit_code=0$")
        if("${successful_exit}" STREQUAL "")
            message(FATAL_ERROR
                "${expected_platform} layout evidence has no successful exit")
        endif()
        foreach(kind IN ITEMS wide narrow)
            file(STRINGS
                "${report_path}"
                screenshot_line
                REGEX "^screenshot_${kind}_path=.+$")
            string(REGEX REPLACE
                "^screenshot_${kind}_path=" "" screenshot_relative
                "${screenshot_line}")
            get_filename_component(evidence_directory "${report_path}" DIRECTORY)
            get_filename_component(change_directory "${evidence_directory}" DIRECTORY)
            set(screenshot "${change_directory}/${screenshot_relative}")
            if(NOT EXISTS "${screenshot}")
                message(FATAL_ERROR
                    "${expected_platform} ${kind} layout screenshot is missing")
            endif()
            file(SIZE "${screenshot}" screenshot_size)
            if(screenshot_size LESS 4096)
                message(FATAL_ERROR
                    "${expected_platform} ${kind} layout screenshot is too small")
            endif()
        endforeach()
    endif()
endfunction()

validate_evidence("${linux_evidence_path}" linux windows)
validate_evidence("${windows_evidence_path}" windows linux)
