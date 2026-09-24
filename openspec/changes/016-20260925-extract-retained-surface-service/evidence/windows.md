# Retained surface Windows 集成验收

schema_version=1
change=016-20260925-extract-retained-surface-service
scope=windows
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug,windows-msvc-release
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=e7fc57460dd038f5fef73f629f6d2d3ccfc3642d
ctest_debug=227/227
ctest_release_affected=14/14
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
font_families=Segoe_UI_Variable_Text,Microsoft_YaHei_UI
host_display_scale=1.5
acceptance_scale=1.5
animation_acceptance=passed
search_acceptance=passed
selection_acceptance=passed
input_acceptance=passed
manual_visual_review=passed
search_screenshot_sha256=8e8386090fe4512a256201031d255e2fa5000bfda474c5a9267f0c0b9da09c2a
selection_screenshot_sha256=0379d3029e6d37589f7f4ed738ad839350750dbae5a71ced301aea6f7efaaf12
native_ime_014=pending
exit_code=0

2026-09-25，正式 `windows-msvc` Debug/Release 构建通过；Debug 全量 CTest 227/227，Release 受影响的 Button/Input/Search/Selection/ReferenceSurface/Gallery 等 14 项 CTest 14/14 通过。Release Token Gallery 在真实 Win32 窗口以 1.5 acceptance scale 分别运行 Button 动画、Search、Selection 和 Input 自动流程，四项正常退出。诊断记录 D3D12、DXIL、系统字体及实际宿主显示缩放 1.5；该信息确认图形后端，不推断物理 GPU 型号。

`windows/` 保存四项诊断及 Search/Selection 的 client 区截图。人工查看截图，Search 的输入、焦点、loading 与禁用状态，Switch/Checkbox 的选中、半选与 CJK/Latin 文本均清晰，未见组件交错或裁剪。Input 诊断覆盖 selection、clipboard、undo/redo、Theme 状态和 caret idle；动画诊断覆盖焦点与键盘激活。这里验证的是内部 surface 提取的回归，不代替 014 仍待用户验证的 Windows 原生 IME 候选窗与 composition。

Linux 原生 Wayland、Vulkan/SPIR-V、Fontconfig 和实窗输入验收仍单列待验。
