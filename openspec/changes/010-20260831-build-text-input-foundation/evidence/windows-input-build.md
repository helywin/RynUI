# Windows Input build evidence

scope=windows
build_result=passed
manual_ime_visual_result=pending
source_sha=aa091d53fad0ef34d66baafd4f22e8db69c62022

configure=fresh
preset=windows-msvc
generator=Ninja Multi-Config
compiler=MSVC 19.51.36256.0
architecture=x64
cpp_standard=C++20
dependency_mode=BUNDLED
utf8proc_version=2.11.3
sdl_version=3.4.14
debug_build=passed
debug_ctest=207/207
debug_ctest_elapsed_seconds=312.90
release_build=passed
release_ctest=207/207
release_ctest_elapsed_seconds=119.06

window_system=win32
text_input=SDL_StartTextInputWithProperties,SDL_SetTextInputArea
clipboard=SDL_HasClipboardText,SDL_GetClipboardText,SDL_SetClipboardText
system_font_discovery=DirectWrite
font_source=system
font_families=Segoe UI Variable Text,Microsoft YaHei UI
gpu_driver=direct3d12
shader_compiler=DirectXShaderCompiler 1.8.2502 locked archive
shader_format=DXIL
shader_runtime=dxcompiler.dll,dxil.dll
generated_shaders=passed
deployed_shaders=passed
dependency_lock=passed

manual_pending=Windows acceptance runner and direct system IME/candidate/visual verification at 1.0,1.25,1.5,2.0 scales
screenshot_path=not-captured
screenshot_reason=not-required-for-build-evidence

2026-09-19 在 Windows 11 x64 上从 `windows-msvc` fresh configure 开始，使用 MSVC 与 Ninja Multi-Config 完成 Debug、Release build 和两套完整 CTest。配置日志确认锁定的 BUNDLED utf8proc/SDL3，SDL Windows video backend 与 D3D12 GPU driver；构建日志确认锁定 DXC 生成 DXIL，并把 `dxcompiler.dll`、`dxil.dll` 部署到 Release runtime。

本记录只关闭 9.1 自动构建边界。Win32 SDL text input/clipboard adapter 与 DirectWrite system font discovery 已由对应平台分支测试执行；真实窗口 acceptance runner、系统中文 IME、候选窗位置和人工视觉仍为 pending，不由本记录宣称通过。
