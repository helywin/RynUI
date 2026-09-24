# Button 窗口服务访问边界 Windows 集成验收

schema_version=1
change=018-20260925-remove-button-window-service-friend
scope=windows
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug,windows-msvc-release
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=6884e08d3b518270c62398fc9fd34734391a4b9c
ctest_debug_affected=14/14
ctest_release_affected=14/14
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
font_families=Segoe_UI_Variable_Text,Microsoft_YaHei_UI
host_display_scale=1.5
acceptance_scale=1.5
animation_acceptance=passed
keyboard_activations=1
focus_traversals=1
animation_frames=204
native_ime_014=pending
exit_code=0

2026-09-25，正式 `windows-msvc` Debug/Release 构建和相同的 14 项受影响 CTest 均通过（Debug 19.52 秒，Release 8.27 秒）。Release Token Gallery 在真实 Win32 窗口以 1.5 acceptance scale 跑完 Button 动画自动流程，诊断显示一次键盘激活、一次焦点遍历、204 个动画帧并正常退出。`windows/animation-scale-1.5.txt` 保存 D3D12、DXIL、系统字体、实际宿主缩放与退出码。这里只确认图形后端，不推断物理 GPU 型号。

这是纯内部访问边界回归，没有新 OS/GPU 行为；Linux 原生验证依用户安排暂缓。此流程不验证 014 仍待实际验证的 Windows 原生 IME 候选窗与 composition。
