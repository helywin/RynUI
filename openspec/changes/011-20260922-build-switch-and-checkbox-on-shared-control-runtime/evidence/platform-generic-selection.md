# Switch/Checkbox 平台通用验收

schema_version=1
change=011-20260922-build-switch-and-checkbox-on-shared-control-runtime
scope=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
source_version=6.6.5
dependency_mode=BUNDLED
feature_commit_sha=d9c682df7b9bdb6deb411f7ffe3ca69e155c76d3
full_ctest_debug=216/216
source_contract_exit_code=0
selection_component_exit_code=0
idle_benchmark_exit_code=0
idle_iterations=10000
idle_animation_updates=0
idle_scene_rebuilds=0
idle_material_updates=0
git_diff_check_exit_code=0
platform_behavior=not-required-platform-generic
exit_code=0

2026-09-25，在正式 `windows-msvc` preset 的 Debug 配置执行 `scripts/build-windows.ps1 -Configuration Debug`，完整 CTest 216/216 通过（217.42 秒）。其中包含 Switch/Checkbox 来源与公开 API、Button/Input 同窗回归、主题和 scene、依赖锁与 shader 合同、公开头编译、Python cache 检查。`selection_idle_benchmark` 运行 10000 次空闲迭代，动画更新、scene 重建和材质更新均为 0。

本证据只记录平台通用合同的一次 Windows 执行。Linux 原生窗口、GPU、字体和输入验收单列在 tasks.md，尚未完成。
