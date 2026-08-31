cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "Windows Gallery build EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" evidence)
foreach(required IN ITEMS
        "scope=windows"
        "build_result=passed"
        "manual_visual_result=pending"
        "configure=fresh"
        "preset=windows-msvc"
        "generator=Ninja Multi-Config"
        "compiler=MSVC 19.51.36256.0"
        "architecture=x64"
        "dependency_mode=BUNDLED"
        "debug_ctest=166/166"
        "release_ctest=166/166"
        "window_system=win32"
        "wheel_input=passed"
        "system_font_discovery=DirectWrite"
        "font_source=system"
        "gpu_driver=direct3d12"
        "shader_format=DXIL"
        "release_runtime_exit=0"
        "dependency_lock=passed"
        "python_cache=clean")
    string(FIND "${evidence}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows Gallery build evidence is missing: ${required}")
    endif()
endforeach()
if(evidence MATCHES "manual_visual_result=passed")
    message(FATAL_ERROR "Automated Windows build evidence claimed manual visual acceptance")
endif()
