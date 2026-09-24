# 窗口服务独立编译平台通用验收

schema_version=1
change=017-20260925-decouple-window-services-from-button-source
scope=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=6421fe43889eec9b1af73ce5bfd5d33012dcca03
ctest_debug=227/227
window_service_definitions_new_tu=16
window_service_definitions_button_tu=0
method_body_equivalence=passed
mixed_window_lifecycle=passed
idle_benchmarks=passed
dependency_lock_and_license=passed
generated_and_deployed_shaders=passed
python_cache_clean=passed
public_header_contracts=passed
exit_code=0

2026-09-25，迁移前后的 16 个 `WindowComponentServices::` 方法定义文本比较一致，Button 源文件中不再有窗口服务方法定义；`src/CMakeLists.txt` 显式编译新翻译单元。正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug 构建和完整 CTest 227/227 通过（217.00 秒）。Button/Input/Search/Selection 混合窗口、scene、生命周期和空闲基准均在这次测试内通过；公开头、依赖锁/license、shader 与 Python cache 合同也通过。

这是平台通用合同在一个受支持正式 preset 的执行结果。Linux 的 OS/GPU/系统字体与真实窗口行为另列待验。
