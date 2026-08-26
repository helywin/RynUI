foreach(required_variable IN ITEMS EVIDENCE_REPORT EVIDENCE_SCREENSHOT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${EVIDENCE_REPORT}")
    message(FATAL_ERROR "Windows text evidence report is missing")
endif()
if(NOT EXISTS "${EVIDENCE_SCREENSHOT}")
    message(FATAL_ERROR "Windows text evidence screenshot is missing")
endif()
file(SIZE "${EVIDENCE_SCREENSHOT}" screenshot_size)
if(screenshot_size LESS 1024)
    message(FATAL_ERROR "Windows text evidence screenshot is unexpectedly small")
endif()

file(READ "${EVIDENCE_REPORT}" report)
foreach(required IN ITEMS
        "windows-msvc-debug"
        "windows-msvc-release"
        "Ninja Multi-Config"
        "gpu_driver=direct3d12"
        "shader_format=DXIL"
        "replacement_count=0"
        "material_updates=1"
        "exit_code=0"
        "content、color、width constraint 与 resize"
        "14px"
        "Ant Design 6.5")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows text evidence is missing token: ${required}")
    endif()
endforeach()
