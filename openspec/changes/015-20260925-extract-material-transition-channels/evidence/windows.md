# 材质过渡通道 Windows 集成验收

schema_version=1
change=015-20260925-extract-material-transition-channels
scope=windows
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug,windows-msvc-release
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=09b6a84239db6362f72ef10cfb89b39cc3594e57
ctest_debug=227/227
ctest_release=227/227
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
host_display_scale=1.5
acceptance_scale=1.5
animation_acceptance=passed
search_acceptance=passed
selection_acceptance=passed
input_acceptance=passed
manual_visual_review=passed
search_screenshot_sha256=c30bcb912bfd88a688d3a1545a8ebf3f8f3c3f368acdbd7fcbbb005b13246597
selection_screenshot_sha256=d625e39140dac382e7e8e5b8e9970e0e4f6004e50904fa9980c3d99e1b4b0152
native_ime_014=pending
exit_code=0

2026-09-25，正式 `windows-msvc` preset 的 Debug/Release 全量 CTest 各 227/227 通过（Debug 217.94 秒、Release 89.54 秒）。Release Token Gallery 在真实 Win32 窗口以 1.5 acceptance scale 分别运行既有 Button 动画、Search、Selection 和 Input 自动流程，四项均正常退出。日志记录 D3D12、DXIL、系统字体 `Segoe UI Variable Text`/`Microsoft YaHei UI`、实际宿主显示缩放 1.5；只确认图形后端，不推断物理 GPU 型号。

`windows/` 保存四项诊断与 Search/Selection client 区截图。动画诊断显示焦点遍历、键盘激活及退出码 0；Search 的 Enter/按钮/禁用门禁、Input 的编辑/剪贴板/撤销重做、Selection 的按压和焦点诊断也均通过。人工查看两张截图，Search 的输入/按钮、焦点与 loading，Switch/Checkbox 的状态和 CJK/Latin 文本清晰且未见相互裁剪。此处是内部材质过渡重构的回归，不代替 014 仍待用户验证的 Windows 原生 IME 候选窗与 composition。

Linux 原生 Wayland、Vulkan/SPIR-V、Fontconfig 和实窗输入验收仍单列待验。
