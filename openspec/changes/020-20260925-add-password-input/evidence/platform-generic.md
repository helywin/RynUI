# Password 平台通用实现验证

- Windows 10.0.26200 x64，Visual Studio 2026 MSVC 14.51.36231，正式 `windows-msvc-debug`、`Ninja Multi-Config`。核心实现及独立 Password 测试加入后，`pwsh -NoProfile -File scripts/build-windows.ps1 -Configuration Debug` 完整构建和 CTest 230/230 通过。
- Password 定向测试覆盖复杂 Unicode 字素遮罩、组合输入映射、受控可见性回写、隐藏状态复制/剪切阻止、粘贴、指针切换焦点与组合输入保持、键盘切换、禁用和销毁清理；原 Input 展示、组件、指针、键盘及文本会话回归通过。
- 一次直接运行 `ctest` 的全量尝试因未进入 MSVC Developer Shell，公开头与依赖配置测试缺少编译器标准库路径而失败；随后用正式脚本重跑 230/230 通过。此记录不把前一轮环境失败计为产品失败。
- 本文件记录核心实现提交前的结果；Gallery 后续调整及原生窗口验收在 Windows 专属证据中另记。Linux 原生结果待后续实际 Linux 机器验收。
