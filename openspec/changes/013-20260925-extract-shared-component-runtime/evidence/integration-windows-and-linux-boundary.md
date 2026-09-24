# 集成验收与平台边界

## 平台通用合同（实际在 Windows 执行）

- 日期：2026-09-25；Windows 11 10.0.26200；Visual Studio 2026 MSVC 19.51.36256.0 x64；`windows-msvc` / `Ninja Multi-Config` / `BUNDLED`。
- 最终按压实现后，`./scripts/build-windows.ps1 -Configuration Debug`：CTest 212/212 通过，含 Button/Input、Gallery、scene allocation、Pressable allocation、公开头、依赖合同、依赖锁、shader 与 Python cache 检查。`input_scene_allocation` 108.89 秒通过，`pressable_behavior_allocation` 0.02 秒通过。
- `./scripts/build-windows.ps1 -Configuration Release`：CTest 212/212 通过，`input_scene_allocation` 8.97 秒通过。
- 集成复核后增加同窗唯一 `InputComponentHost` 检查，避免第二宿主覆盖挂载上下文；重新完成 Debug/Release 构建，并在两个配置分别运行 Input、journey、Button、Pressable allocation、Gallery 的 5/5 重点回归。完整 212 项结果发生在该窄幅检查之前，不把它描述为检查之后的全量重跑。

## Windows 实窗自动验收

- `./scripts/run-windows-input-acceptance.ps1 -Configuration Release`：1.0、1.25、1.5、2.0 四档 acceptance scale 均退出码 0；每档报告 `window_system=win32`、`gpu_driver=direct3d12`、`shader_format=DXIL`、`input_latin/selection/clipboard/undo/redo/theme_status/caret_idle=passed`。
- 宿主显示缩放为 1.5；系统字体为 Segoe UI Variable Text、Microsoft YaHei UI。Release Gallery `--smoke` 在同四档缩放报告 Win32/D3D12/DXIL 和退出码 0。
- 自动实窗证据只覆盖脚本操作路径；未将其称为 Gallery 全部状态的人工视觉验收，也不替代 008/012 中尚未完成的截图任务。

## Linux 边界

- 本机可用的 Ubuntu 24.04 是 WSL2/WSLg（内核报告 `microsoft-standard-WSL2`），会话没有原生 Linux 桌面类型且未安装 Clang。`linux-gcc-debug` 的辅助构建在重配后开始编译，因需重建大量目标而停止，未产生通过结果。
- 013 的原生 Linux Wayland、GCC/Clang、Vulkan/SPIR-V、Fontconfig 和真实输入验收仍未完成；Windows 与 WSLg 结果均不代替该 checkbox。
