# Windows libdecor 依赖隔离验收

schema_version=1
change=007-20260828-fix-linux-wayland-libdecor-resize
platform=windows
status=passed
source_revision=b60e8893ba0c67b5c13c1f951c3b351cf7f2d03b
date=2026-08-29
os=Microsoft Windows 11 专业工作站版 10.0.26200 build 26200
preset=windows-msvc
configuration=Debug
cmake=4.3.1-msvc1
generator=Ninja Multi-Config
compiler=MSVC 19.51.36256.0 x64
architecture=x64
dependency_mode=BUNDLED
sdl3=3.4.14
libdecor_downloads=0
libdecor_targets=0
libdecor_build_entries=0
libdecor_binary_dependencies=0
configure_exit_code=0
build_exit_code=0
dependency_tests=3/3
public_header_isolation_tests=6/6
d3d12_token_gallery_smoke=4/4
full_ctest=132/132
exit_code=0

最终 `cmake --fresh --preset windows-msvc` 只解析锁定的 SDL3、DXC、SPIRV-Cross、SDL_shadercross、FreeType 与 HarfBuzz；配置输出没有 patched libdecor resolver，SDL 明确报告 `SDL_WAYLAND=OFF` 与 `SDL_WAYLAND_LIBDECOR=OFF`。CMake configure 阶段直接拒绝任何 `RynUI::LibDecor` 或 `rynui_libdecor_external` target，`rynui.windows_libdecor_isolation` 进一步检查 Debug build graph 与 `_deps`，未发现 libdecor feature define、staging/build/source 目录或 ELF 名称。

Debug 构建使用 MSVC x64 与 Ninja Multi-Config。`dumpbin /dependents` 检查 `rynui_minimal.exe` 和 `rynui_token_gallery.exe`，依赖均为 Windows、MSVC runtime 与 `DWrite.dll` 等正常 DLL，`libdecor` 命中数为 0。

共享平台、字体与 glyph 代码也在该 Linux 修复系列中发生变化，因此额外以真实 D3D12/DXIL Token Gallery smoke 回归默认窗口 scale 1.5 和 acceptance render scale 1.0、1.5、2.0。四次运行均报告 `gpu_driver=direct3d12`、系统字体 rendering telemetry 与 `exit_code=0`。

首次 fresh configure 暴露了测试注册回归：Linux patch contract 在非 Linux 平台无条件要求 GNU `patch`。本阶段把 patch application、bundled build graph、Wayland pacing 与 Linux resolver cases 限定到 Linux；Windows 只保留 source dependency contract、`non-linux` resolver 与实际 build-graph isolation contract。最终 full CTest 为 132/132，通过结果来自修复后的 Windows fresh build。
