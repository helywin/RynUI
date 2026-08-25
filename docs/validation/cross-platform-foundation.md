# Cross-platform foundation validation

验证日期：2026-08-26

## Linux / WSL 2 / Vulkan

验收环境为 WSL 2 Ubuntu 24.04，内核 `6.18.33.2-microsoft-standard-WSL2`，GCC 13.3、CMake 3.28.3、Ninja 1.11.1。WSLg 提供 Wayland、X11、`DISPLAY=:0`、`WAYLAND_DISPLAY=wayland-0` 和 `/dev/dxg`；SDL3 3.4.14 的 configure 摘要启用了 Wayland/X11 video backend 与 Vulkan GPU driver。

| Check | Command / mode | Result |
| --- | --- | --- |
| configure | `cmake --preset linux-gcc` | PASS，`Ninja Multi-Config`、GCC、`RYNUI_DEPENDENCY_MODE=BUNDLED` |
| build | `cmake --build --preset linux-gcc-debug` | PASS |
| tests | `ctest --preset linux-gcc-debug` | 24/24 PASS |
| timed real-window smoke | `rynui_minimal --smoke` | exit 0，`gpu_driver=vulkan shader_format=SPIR-V component_runs=1 signal_writes=4 observer_executions=8 measure=4 layout=4 primitive_rebuilds=1 instance_updates=3 gpu_uploads=4 gpu_uploaded_bytes=192 submits=4 idle_wakes=0 idle_waits=121` |
| interactive real window | WSLg window close event | exit 0，`gpu_driver=vulkan shader_format=SPIR-V ... submits=7 idle_wakes=2 idle_waits=5735` |

Windows Graphics Capture 对 WSLg 窗口的核对结果为：深色背景上的紫色圆角 Quad，尺寸、位移、圆角和透明度混合与 Windows/D3D12 结果一致。点击窗口关闭按钮后事件循环收到 close event 并正常退出。

## Windows / MSVC / D3D12

验收环境使用 Visual Studio 2026 Developer PowerShell 与 MSVC 19.51。`-Fresh` 重新生成 CMake cache 后，`CMAKE_GENERATOR` 为 `Ninja Multi-Config`，SDL3 3.4.14 启用了 Windows video backend 与 D3D12/Vulkan GPU driver。

| Dependency mode | Configuration | Configure/build/test | Real-window result |
| --- | --- | --- | --- |
| `BUNDLED` | Debug | Fresh configure，24/24 CTest PASS | 已由同一 build tree 完成真实窗口视觉验收 |
| `BUNDLED` | Release | 24/24 CTest PASS | exit 0，`gpu_driver=direct3d12 shader_format=DXIL component_runs=1 signal_writes=4 observer_executions=8 measure=4 layout=4 primitive_rebuilds=1 instance_updates=3 gpu_uploads=4 gpu_uploaded_bytes=192 submits=4 idle_wakes=0 idle_waits=139` |
| `SYSTEM` | Debug | Fresh configure，23/23 CTest PASS | exit 0，`gpu_driver=direct3d12 shader_format=DXIL ... submits=4 idle_waits=138` |

`BUNDLED` cache 明确记录 `RYNUI_DEPENDENCY_MODE=BUNDLED`；`SYSTEM` 独立 build tree 明确记录 `RYNUI_DEPENDENCY_MODE=SYSTEM`、调用方提供的 `SDL3_DIR` 和 `RYNUI_SHADERCROSS_EXECUTABLE`，且不会构建 bundled shadercross override 测试，因此完整测试数为 23。两条路径分别对应文档中的锁定 archive 解析和调用方 CMake package/host tool 解析，没有隐式 fallback。

## OpenSpec 与仓库收口

| Check | Result |
| --- | --- |
| `openspec doctor --json` | `healthy=true`，无 status issue |
| `openspec validate --all --strict --no-interactive` | 1/1 PASS |
| `git diff --check` | PASS |
| change scope | worktree 仅包含本 change 的 README、验收记录和任务状态更新 |
