cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS LINUX_EVIDENCE WINDOWS_EVIDENCE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
    if(NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} report is missing")
    endif()
endforeach()

file(REAL_PATH "${LINUX_EVIDENCE}" linux_path)
file(REAL_PATH "${WINDOWS_EVIDENCE}" windows_path)
if(linux_path STREQUAL windows_path)
    message(FATAL_ERROR "Linux and Windows Token Gallery evidence cannot share one file")
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
    window_system
    gpu_driver
    driver_version
    shader_format
    shader_hash
    catalog_hash
    theme_states
    screenshot_primary_path
    screenshot_secondary_path
    screenshot_tertiary_path
    effect_layers
    outer_layers
    inset_layers
    focus_layers
    quad_uploads
    glyph_uploads
    effect_uploads
    quad_draws
    glyph_draws
    effect_draws
    frame_submissions
    idle_waits
    exit_code)

function(validate_evidence report_path expected_platform other_platform)
    file(READ "${report_path}" report)
    foreach(key IN LISTS required_keys)
        string(FIND "${report}" "${key}=" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} Token Gallery evidence is missing: ${key}")
        endif()
    endforeach()
    string(FIND "${report}" "platform=${expected_platform}" expected_found)
    string(FIND "${report}" "platform=${other_platform}" other_found)
    if(expected_found EQUAL -1 OR NOT other_found EQUAL -1)
        message(FATAL_ERROR
            "${expected_platform} Token Gallery evidence has a cross-platform identity")
    endif()
    foreach(kind IN ITEMS primary secondary tertiary)
        string(FIND
            "${report}"
            "screenshot_${kind}_path=evidence/${expected_platform}-token-gallery-"
            screenshot_found)
        string(FIND
            "${report}"
            "screenshot_${kind}_path=not-required"
            not_required_found)
        if(screenshot_found EQUAL -1 AND not_required_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} Token Gallery screenshot path is not platform-specific")
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
        string(FIND "${report}" "pending" pending_found)
        if(passed_found EQUAL -1 OR NOT pending_found EQUAL -1)
            message(FATAL_ERROR
                "${expected_platform} Token Gallery evidence is not complete")
        endif()
        foreach(counter IN ITEMS
                effect_layers outer_layers inset_layers focus_layers
                quad_uploads glyph_uploads effect_uploads
                quad_draws glyph_draws effect_draws frame_submissions idle_waits)
            file(STRINGS "${report_path}" line REGEX "^${counter}=[0-9]+$")
            if("${line}" STREQUAL "")
                message(FATAL_ERROR
                    "${expected_platform} Token Gallery counter is not numeric: ${counter}")
            endif()
        endforeach()
        file(STRINGS "${report_path}" successful_exit REGEX "^exit_code=0$")
        if("${successful_exit}" STREQUAL "")
            message(FATAL_ERROR
                "${expected_platform} Token Gallery has no successful exit")
        endif()

        get_filename_component(report_directory "${report_path}" DIRECTORY)
        get_filename_component(change_directory "${report_directory}" DIRECTORY)
        foreach(kind IN ITEMS primary secondary tertiary)
            file(STRINGS
                "${report_path}"
                screenshot_line
                REGEX "^screenshot_${kind}_path=")
            string(REGEX REPLACE "^[^=]+=" "" screenshot_path "${screenshot_line}")
            if(NOT screenshot_path STREQUAL "not-required")
                if(IS_ABSOLUTE "${screenshot_path}")
                    set(screenshot_file "${screenshot_path}")
                else()
                    set(screenshot_file "${change_directory}/${screenshot_path}")
                endif()
                if(NOT EXISTS "${screenshot_file}")
                    message(FATAL_ERROR
                        "${expected_platform} Token Gallery screenshot is missing: ${screenshot_file}")
                endif()
                file(SIZE "${screenshot_file}" screenshot_size)
                if(screenshot_size LESS 4096)
                    message(FATAL_ERROR
                        "${expected_platform} Token Gallery screenshot is too small: ${screenshot_file}")
                endif()
            endif()
        endforeach()

        file(STRINGS "${report_path}" shader_line REGEX "^shader_hash=")
        string(REGEX REPLACE "^[^=]+=" "" shader_hash "${shader_line}")
        string(LENGTH "${shader_hash}" shader_hash_length)
        if(NOT shader_hash MATCHES "^[0-9A-Fa-f]+$" OR NOT shader_hash_length EQUAL 64)
            message(FATAL_ERROR
                "${expected_platform} Token Gallery shader hash is not SHA256")
        endif()

        if(expected_platform STREQUAL "windows")
            file(STRINGS
                "${report_path}"
                windows_scales
                REGEX "^display_scale=1\\.0,1\\.5,2\\.0$")
            file(STRINGS
                "${report_path}"
                scale_source
                REGEX "^scale_source=acceptance-render-override$")
            if("${windows_scales}" STREQUAL "" OR "${scale_source}" STREQUAL "")
                message(FATAL_ERROR
                    "Windows Token Gallery evidence does not identify all render scales and their source")
            endif()
        endif()
    endif()
endfunction()

validate_evidence("${linux_path}" linux windows)
validate_evidence("${windows_path}" windows linux)
