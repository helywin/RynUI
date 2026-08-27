foreach(required_variable IN ITEMS EVIDENCE_REPORT EVIDENCE_SCREENSHOT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${EVIDENCE_REPORT}")
    message(FATAL_ERROR "Linux Text component evidence report is missing")
endif()
if(NOT EXISTS "${EVIDENCE_SCREENSHOT}")
    message(FATAL_ERROR "Linux Text component evidence screenshot is missing")
endif()
file(SIZE "${EVIDENCE_SCREENSHOT}" screenshot_size)
if(screenshot_size LESS 4096)
    message(FATAL_ERROR "Linux Text component evidence screenshot is unexpectedly small")
endif()

file(READ "${EVIDENCE_REPORT}" report)
foreach(required IN ITEMS
        "003-20260827-build-text-component-foundation"
        "linux-gcc-debug"
        "linux-gcc-release"
        "Ninja Multi-Config"
        "GCC 16.2.1"
        "RYNUI_DEPENDENCY_MODE=BUNDLED"
        "rynui_freetype-src"
        "rynui_harfbuzz-src"
        "glyph.vertex.spv"
        "glyph.fragment.spv"
        "gpu_driver=vulkan"
        "shader_format=SPIR-V"
        "display_scale=2"
        "mount_runs=1"
        "prop_updates=4"
        "resize_updates=1"
        "replacement_count=0"
        "shape_count=5"
        "measure_count=8"
        "atlas_entries=47"
        "instance_count=107"
        "material_updates=1"
        "geometry_updates=8"
        "submits=7"
        "idle_waits="
        "exit_code=0"
        "content、tone、width、margin 与 resize"
        "primary、secondary、disabled"
        "14 logical-pixel"
        "Ant Design 6.5.0"
        "linux-clang-debug"
        "Clang 22.1.8"
        "/usr/bin/clang++"
        "-std=c++20"
        "55/55 通过"
        "任务 7.2 已完成")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Linux Text component evidence is missing token: ${required}")
    endif()
endforeach()
