# 材质过渡通道平台通用验收

schema_version=1
change=015-20260925-extract-material-transition-channels
scope=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=09b6a84239db6362f72ef10cfb89b39cc3594e57
ctest_debug=227/227
helper_contract=passed
input_scene_allocation=passed
gallery_journey=passed
dependency_lock_and_license=passed
generated_and_deployed_shaders=passed
python_cache_clean=passed
public_header_contracts=passed
exit_code=0

2026-09-25，正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug 构建与全量 CTest 227/227 通过（217.94 秒）。helper 合同覆盖 color/scalar target、注册异常回滚、原地和反向 retarget、同值收敛、scope 销毁、空闲无 deadline 与稳态零分配。既有 Input scene allocation、Button spinner、Search/Selection 混合场景和 Gallery journey 均通过。依赖锁/license、shader、Python cache 与公开头合同包含在全量 CTest 中。

本报告只记录平台通用逻辑合同的一次 Windows 执行；没有以另一平台重复同一合同。OS/GPU 行为见单独 Windows 证据，Linux 原生环境留待用户安排。
