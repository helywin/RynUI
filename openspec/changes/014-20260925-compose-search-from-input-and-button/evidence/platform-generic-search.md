# Search 平台通用验收

schema_version=1
change=014-20260925-compose-search-from-input-and-button
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
feature_commit_sha=c15c5d3d594d2cb8371b056c7c789ca192908c0c
full_ctest_debug=223/223
source_contract_exit_code=0
search_component_exit_code=0
idle_benchmark_exit_code=0
idle_iterations=10000
idle_animation_updates=0
idle_scene_rebuilds=0
idle_material_updates=0
git_diff_check_exit_code=0
platform_behavior=not-required-platform-generic
exit_code=0

2026-09-25，在正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug preset 完成构建，并在同一 Visual Studio Developer Shell 运行平台通用 CTest 223/223 通过（214.83 秒）。覆盖 Search 来源、公开 API、组合状态、同窗 Input/Button/Selection、Gallery journey、依赖锁与 license、生成与部署 shader、Python cache 以及公开头编译合同。`search_idle_benchmark` 的 10000 次空闲迭代中，动画更新、scene 重建、材质更新均为 0。

本报告只计平台通用合同的一次 Windows 执行。另加的 evidence schema 测试在报告写入后单独验收；Windows 实窗和 Linux 原生环境的行为独立记录。
