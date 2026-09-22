# Ant Design 6.6.5 Windows 构建与自动实窗验证

- 环境：Windows 10.0.26200；`windows-msvc` 正式 preset、Ninja Multi-Config、MSVC x64 19.51.36256。系统显示缩放 1.5；系统枚举的显示设备包含 NVIDIA GeForce RTX 5080（驱动 32.0.16.1656）、OrayIddDriver Device 和 GameViewer Virtual Display Adapter。应用诊断报告 `window_system=win32`、`gpu_driver=direct3d12`、`shader_format=DXIL`，但未由此推断实际选择了哪张物理显卡。
- `scripts/build-windows.ps1 -Configuration Debug`：构建及 CTest 211/211 PASS，256.98 秒。
- `scripts/build-windows.ps1 -Configuration Release`：构建及 CTest 211/211 PASS，89.54 秒。先前直接在普通 PowerShell 运行的 Release 子集 47/49 通过，两个 compile-fail contract 因缺少 VS SDK `rc` 失败；在正式 VS Developer Environment 完整重跑后全部通过，不计作产品缺陷。
- `scripts/run-windows-input-acceptance.ps1 -Configuration Release`：1.0、1.25、1.5、2.0 四档 acceptance scale 均退出 0。诊断包含 Win32/D3D12/DXIL、系统字体 `Segoe_UI_Variable_Text,Microsoft_YaHei_UI`，以及 Latin 输入、选择、剪贴板、undo/redo、Theme 状态、光标 idle 全部 `passed`。原始日志位于被忽略的 `out/acceptance/windows-input/scale-*.txt`。
- Release Gallery 使用 `--smoke --acceptance-scale` 逐档运行 1.0、1.25、1.5、2.0，四档均有 Win32/D3D12/DXIL 诊断且退出 0。此 smoke 会实际创建窗口并运行自动流程，但不逐项目视检查目录和状态。
- 阶段 7.2 的 Button/Input/Gallery 人工视觉、全部目录/状态交互和新版截图尚未完成；本文件不作为完整 Windows passed evidence，也不复用 6.5.0 截图。阶段 7.3 保持未完成。
