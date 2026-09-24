# 窗口服务独立编译 Windows 集成验收

schema_version=1
change=017-20260925-decouple-window-services-from-button-source
scope=windows
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug,windows-msvc-release
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=6421fe43889eec9b1af73ce5bfd5d33012dcca03
ctest_debug=227/227
ctest_release_affected=13/13
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
font_families=Segoe_UI_Variable_Text,Microsoft_YaHei_UI
host_display_scale=1.5
acceptance_scale=1.5
search_acceptance=passed
manual_visual_review=passed
search_screenshot_sha256=df715f8df01019fce553fce79a07a3c7dae95b18e155b97f42c99f1c4545c7df
native_ime_014=pending
exit_code=0

2026-09-25，正式 `windows-msvc` Debug/Release 构建通过；Debug 全量 CTest 227/227，Release 受影响的 Button/Input/Search/Selection/ReferenceSurface/Gallery 等 13 项 CTest 13/13 通过。Release Token Gallery 在真实 Win32 窗口以 1.5 acceptance scale 运行 Search 自动流程，诊断中的 Enter、按钮、disabled/loading 门禁和退出码均正常。日志记录 D3D12、DXIL、系统字体及宿主显示缩放 1.5；只确认图形后端，不推断物理 GPU 型号。

`windows/search-scale-1.5.txt` 和截图保存本次实窗证据。人工查看截图，Input 焦点、Search 两档按钮和 loading/disabled、相邻 Switch/Checkbox 及 CJK/Latin 文本没有可见交错或裁剪。本轮只是窗口服务翻译单元迁移，不代替 014 仍待实际验证的 Windows 原生 IME 候选窗与 composition。

Linux 原生 Wayland、Vulkan/SPIR-V、Fontconfig 和真实窗口验收暂缓。
