# InputAffixAction Windows 验收

- 环境：Windows 10.0.26200 x64、Visual Studio 2026 MSVC 14.51.36231；`windows-msvc` Ninja Multi-Config preset。
- 正式 Release 构建 `pwsh -NoProfile -File scripts/build-windows.ps1 -Configuration Release -SkipTests` 通过；受影响 CTest 12/12 通过，包含 Password、Input、Search、Pointer、Keyboard、Focus 与 Gallery。
- 真实 Win32 Gallery 使用 Debug 构建，Default/Dark 主题各在 1.5 倍 acceptance scale 运行。两个流程的 `password_hidden`、`password_pointer`、`password_keyboard`、`password_disabled`、`password_scroll` 和 `exit_code=0` 均成立；诊断报告 `gpu_driver=direct3d12`、`shader_format=DXIL`、`window_system=win32`，字体来源为系统 `Segoe UI Variable Text` 与 `Microsoft YaHei UI`。stderr 均为空。
- 目视检查两张真实窗口截图：遮罩、显示操作、焦点边框与禁用颜色正常。截图与原始诊断位于 `evidence/screenshots/windows-default-scale-1.5.*`、`windows-dark-scale-1.5.*`。
- 自动流程使用应用内注入的组合输入事件，不代表人工操作的 Windows 中文 IME 候选窗验收。Linux 原生验证按用户要求后置。
