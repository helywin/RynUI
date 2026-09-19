cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "Windows Input build EVIDENCE is required")
endif()

file(READ "${EVIDENCE}" evidence)
foreach(required IN ITEMS
        "scope=windows"
        "build_result=passed"
        "manual_ime_visual_result=pending"
        "configure=fresh"
        "preset=windows-msvc"
        "generator=Ninja Multi-Config"
        "compiler=MSVC 19.51.36256.0"
        "architecture=x64"
        "cpp_standard=C++20"
        "dependency_mode=BUNDLED"
        "utf8proc_version=2.11.3"
        "sdl_version=3.4.14"
        "debug_build=passed"
        "debug_ctest=207/207"
        "release_build=passed"
        "release_ctest=207/207"
        "window_system=win32"
        "text_input=SDL_StartTextInputWithProperties,SDL_SetTextInputArea"
        "clipboard=SDL_HasClipboardText,SDL_GetClipboardText,SDL_SetClipboardText"
        "system_font_discovery=DirectWrite"
        "font_source=system"
        "gpu_driver=direct3d12"
        "shader_format=DXIL"
        "shader_runtime=dxcompiler.dll,dxil.dll"
        "generated_shaders=passed"
        "deployed_shaders=passed"
        "dependency_lock=passed")
    string(FIND "${evidence}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows Input build evidence is missing: ${required}")
    endif()
endforeach()

if(NOT evidence MATCHES "source_sha=[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]")
    message(FATAL_ERROR "Windows Input build evidence requires a full source SHA")
endif()
if(evidence MATCHES "manual_ime_visual_result=passed")
    message(FATAL_ERROR "Automated Windows build evidence claimed manual IME/visual acceptance")
endif()
