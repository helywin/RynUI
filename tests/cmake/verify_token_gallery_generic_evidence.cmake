cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR "${EVIDENCE}" STREQUAL "" OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "Token Gallery platform-generic evidence is missing")
endif()
file(READ "${EVIDENCE}" report)
foreach(required IN ITEMS
        "schema_version=1"
        "change=006-20260827-establish-ant-design-token-system"
        "scope=platform-generic"
        "status=passed"
        "os="
        "compiler="
        "preset="
        "build_system=Ninja Multi-Config"
        "catalog_hash="
        "unit_tests_exit_code=0"
        "headless_tests_exit_code=0"
        "contract_tests_exit_code=0"
        "benchmark_tests_exit_code=0"
        "openspec_validate_exit_code=0"
        "openspec_doctor_healthy=true"
        "git_diff_check_exit_code=0"
        "exit_code=0")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Token Gallery platform-generic evidence is missing: ${required}")
    endif()
endforeach()
