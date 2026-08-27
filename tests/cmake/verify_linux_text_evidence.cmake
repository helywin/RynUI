foreach(required_variable IN ITEMS EVIDENCE_REPORT EVIDENCE_SCREENSHOT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${EVIDENCE_REPORT}")
    message(FATAL_ERROR "Linux text evidence report is missing")
endif()
if(NOT EXISTS "${EVIDENCE_SCREENSHOT}")
    message(FATAL_ERROR "Linux text evidence screenshot is missing")
endif()
file(SIZE "${EVIDENCE_SCREENSHOT}" screenshot_size)
if(screenshot_size LESS 1024)
    message(FATAL_ERROR "Linux text evidence screenshot is unexpectedly small")
endif()

file(READ "${EVIDENCE_REPORT}" report)
foreach(required IN ITEMS
        "linux-gcc-debug"
        "linux-gcc-release"
        "Ninja Multi-Config"
        "GCC 13.3.0"
        "RYNUI_DEPENDENCY_MODE=BUNDLED"
        "rynui_freetype-src"
        "rynui_harfbuzz-src"
        "glyph.vertex.spv"
        "glyph.fragment.spv"
        "gpu_driver=vulkan"
        "shader_format=SPIR-V"
        "replacement_count=0"
        "fallback_runs=3"
        "material_updates=1"
        "exit_code=0"
        "content、color、width constraint 与 resize"
        "14px"
        "Ant Design 6.5"
        "linux-clang-debug"
        "Clang 22.1.8"
        "/usr/bin/clang++"
        "-std=c++20"
        "CTest 43/43 通过"
        "任务 7.2 已完成")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Linux text evidence is missing token: ${required}")
    endif()
endforeach()
