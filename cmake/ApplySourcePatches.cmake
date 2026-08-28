cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR is required when applying dependency patches.")
endif()
if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "Patch source directory does not exist: ${SOURCE_DIR}")
endif()
if(NOT DEFINED PATCHES OR PATCHES STREQUAL "")
    set(PATCHES)
    foreach(patch_index RANGE 1 9)
        if(DEFINED PATCH_${patch_index} AND NOT "${PATCH_${patch_index}}" STREQUAL "")
            list(APPEND PATCHES "${PATCH_${patch_index}}")
        endif()
    endforeach()
endif()
if(NOT PATCHES)
    message(FATAL_ERROR "PATCHES or PATCH_1..PATCH_9 must name at least one patch file.")
endif()

if(DEFINED EXPECTED_VERSION_FILE AND NOT EXPECTED_VERSION_FILE STREQUAL "")
    if(NOT EXISTS "${SOURCE_DIR}/${EXPECTED_VERSION_FILE}")
        message(FATAL_ERROR
            "Dependency version file does not exist: ${SOURCE_DIR}/${EXPECTED_VERSION_FILE}")
    endif()
    file(READ "${SOURCE_DIR}/${EXPECTED_VERSION_FILE}" dependency_version_text)
    if(NOT DEFINED EXPECTED_VERSION_PATTERN
            OR NOT dependency_version_text MATCHES "${EXPECTED_VERSION_PATTERN}")
        message(FATAL_ERROR
            "Dependency source does not match expected version pattern "
            "'${EXPECTED_VERSION_PATTERN}'.")
    endif()
endif()

foreach(source_index RANGE 1 9)
    if(DEFINED EXPECTED_SOURCE_FILE_${source_index}
            AND DEFINED EXPECTED_SOURCE_SHA256_${source_index})
        set(source_file
            "${SOURCE_DIR}/${EXPECTED_SOURCE_FILE_${source_index}}")
        if(NOT EXISTS "${source_file}")
            message(FATAL_ERROR "Expected dependency source file is missing: ${source_file}")
        endif()
        file(SHA256 "${source_file}" actual_source_sha256)
        if(NOT actual_source_sha256 STREQUAL
                "${EXPECTED_SOURCE_SHA256_${source_index}}"
                AND NOT ALLOW_ALREADY_APPLIED)
            message(FATAL_ERROR
                "Dependency source file ${EXPECTED_SOURCE_FILE_${source_index}} "
                "does not match its locked pristine SHA256.")
        endif()
    endif()
endforeach()

foreach(patch_index RANGE 1 9)
    if(DEFINED PATCH_${patch_index}
            AND DEFINED EXPECTED_PATCH_SHA256_${patch_index})
        file(SHA256 "${PATCH_${patch_index}}" actual_patch_sha256)
        if(NOT actual_patch_sha256 STREQUAL
                "${EXPECTED_PATCH_SHA256_${patch_index}}")
            message(FATAL_ERROR
                "Dependency patch ${patch_index} SHA256 mismatch: "
                "expected ${EXPECTED_PATCH_SHA256_${patch_index}}, "
                "got ${actual_patch_sha256}.")
        endif()
    endif()
endforeach()

if(NOT DEFINED PATCH_EXECUTABLE OR PATCH_EXECUTABLE STREQUAL "")
    find_program(PATCH_EXECUTABLE patch REQUIRED)
endif()

foreach(patch_file IN LISTS PATCHES)
    if(NOT EXISTS "${patch_file}")
        message(FATAL_ERROR "Dependency patch does not exist: ${patch_file}")
    endif()

    if(ALLOW_ALREADY_APPLIED)
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" --dry-run --batch --force --reverse -p1
                -i "${patch_file}"
            WORKING_DIRECTORY "${SOURCE_DIR}"
            RESULT_VARIABLE reverse_check_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(reverse_check_result EQUAL 0)
            continue()
        endif()
    endif()

    execute_process(
        COMMAND "${PATCH_EXECUTABLE}" --dry-run --batch --force --forward -p1
            -i "${patch_file}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE check_result
        OUTPUT_VARIABLE check_output
        ERROR_VARIABLE check_error
    )
    if(NOT check_result EQUAL 0)
        message(FATAL_ERROR
            "Dependency patch no longer applies cleanly: ${patch_file}\n"
            "${check_output}${check_error}")
    endif()

    execute_process(
        COMMAND "${PATCH_EXECUTABLE}" --batch --force --forward -p1
            -i "${patch_file}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE apply_result
        OUTPUT_VARIABLE apply_output
        ERROR_VARIABLE apply_error
    )
    if(NOT apply_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to apply dependency patch: ${patch_file}\n"
            "${apply_output}${apply_error}")
    endif()
endforeach()
