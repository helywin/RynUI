# Retained surface 平台通用验收

schema_version=1
change=016-20260925-extract-retained-surface-service
scope=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=e7fc57460dd038f5fef73f629f6d2d3ccfc3642d
ctest_debug=227/227
retained_surface_contract=passed
mixed_window_single_store=passed
input_scene_allocation=passed
search_and_selection_idle=passed
gallery_journey=passed
dependency_lock_and_license=passed
generated_and_deployed_shaders=passed
python_cache_clean=passed
public_header_contracts=passed
exit_code=0

2026-09-25，正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug 构建与全量 CTest 227/227 通过（216.46 秒）。通用 surface 的 stale ID、scene 顺序、effect、dirty/upload 测试和 Button/Input/Switch/Checkbox 同窗唯一 store 断言通过；Search/Selection 空闲基准、Input scene allocation 和 Gallery journey 也包含在本次全量测试中。依赖锁与 license、shader、Python cache 和公开头合同均在同一次 CTest 中通过。

曾从未进入 VS Developer Shell 的普通 PowerShell 直接启动 CTest；其中 20 个嵌套 CMake 合同测试因误用 Strawberry 工具链或缺少 MSVC 资源工具失败。随后使用仓库规定的 `scripts/build-windows.ps1 -Configuration Debug` 进入 VS Developer Shell，完整 227 项均通过。本报告仅以正式脚本结果作为验收依据。平台通用合同只执行一次，Linux 平台特有项目留待用户安排。
