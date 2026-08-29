cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR NOT EXISTS "${RYNUI_SOURCE_DIR}/.git")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR must identify the Git worktree")
endif()
if(NOT DEFINED GIT_EXECUTABLE OR NOT EXISTS "${GIT_EXECUTABLE}")
    message(FATAL_ERROR "GIT_EXECUTABLE must identify the configured Git executable")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" ls-files --cached --others --exclude-standard
    WORKING_DIRECTORY "${RYNUI_SOURCE_DIR}"
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE candidates
    ERROR_VARIABLE git_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT git_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect Python cache candidates: ${git_error}")
endif()

string(REPLACE "\\" "/" normalized "${candidates}")
if(normalized MATCHES "(^|\n)([^\n]*/)?__pycache__/"
        OR normalized MATCHES "(^|\n)[^\n]*\\.(pyc|pyo)($|\n)")
    message(FATAL_ERROR
        "Tracked or non-ignored untracked Python cache artifact found:\n${candidates}")
endif()
