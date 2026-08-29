cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE_FILE OR "${EVIDENCE_FILE}" STREQUAL "")
    message(FATAL_ERROR "EVIDENCE_FILE is required.")
endif()
if(NOT EXISTS "${EVIDENCE_FILE}")
    message(FATAL_ERROR "Windows libdecor evidence does not exist: ${EVIDENCE_FILE}")
endif()

file(READ "${EVIDENCE_FILE}" evidence)
foreach(required_key IN ITEMS
        schema_version change platform status source_revision date os preset
        configuration cmake generator compiler architecture dependency_mode sdl3
        libdecor_downloads libdecor_targets libdecor_build_entries
        libdecor_binary_dependencies configure_exit_code build_exit_code
        dependency_tests public_header_isolation_tests d3d12_token_gallery_smoke
        full_ctest exit_code)
    string(FIND "${evidence}" "${required_key}=" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows libdecor evidence is missing: ${required_key}")
    endif()
endforeach()

foreach(expected IN ITEMS
        "change=007-20260828-fix-linux-wayland-libdecor-resize"
        "platform=windows"
        "status=passed"
        "preset=windows-msvc"
        "configuration=Debug"
        "generator=Ninja Multi-Config"
        "compiler=MSVC 19.51.36256.0 x64"
        "dependency_mode=BUNDLED"
        "libdecor_downloads=0"
        "libdecor_targets=0"
        "libdecor_build_entries=0"
        "libdecor_binary_dependencies=0"
        "configure_exit_code=0"
        "build_exit_code=0"
        "dependency_tests=3/3"
        "public_header_isolation_tests=6/6"
        "d3d12_token_gallery_smoke=4/4"
        "full_ctest=132/132"
        "exit_code=0")
    string(FIND "${evidence}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows libdecor evidence mismatch: ${expected}")
    endif()
endforeach()

string(FIND "${evidence}" "pending" pending_found)
if(NOT pending_found EQUAL -1)
    message(FATAL_ERROR "Windows libdecor evidence still contains pending state.")
endif()

file(STRINGS "${EVIDENCE_FILE}" revision_line REGEX "^source_revision=[0-9a-f]+$")
string(REGEX REPLACE "^[^=]+=" "" revision "${revision_line}")
string(LENGTH "${revision}" revision_length)
if(NOT revision_length EQUAL 40)
    message(FATAL_ERROR "Windows libdecor evidence source revision is not a full Git SHA.")
endif()

message(STATUS "Windows libdecor dependency evidence is complete")
