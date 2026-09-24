# Input allowClear Windows 验收

- 环境：Windows 10.0.26200 x64，Visual Studio 2026 MSVC 14.51.36231；正式 `windows-msvc` Ninja Multi-Config preset。Debug 与 Release 全目标构建通过，Release 相关 CTest 10/10 通过；Debug 的 Gallery、Input、Password、Search 回归通过。
- 新脚本 `scripts/run-windows-input-clear-acceptance.ps1` 在真实 Win32 Gallery 的 Default/Dark 主题各运行一次 1.5 倍 acceptance scale。自动流程覆盖非空 Input 的指针清空、重新输入后的 Space 清空、禁用时阻止清空、重新启用、页面滚动及正常退出。两个诊断均报告 `input_clear_scroll/pointer/keyboard/disabled=true`、`exit_code=0`，stderr 为空。
- 诊断确认 D3D12、DXIL、Win32 和系统字体 `Segoe UI Variable Text`、`Microsoft YaHei UI`。两张截图已目视检查，清空符号、焦点边框与 Default/Dark 输入框布局正常。截图和原始诊断位于 `evidence/screenshots/windows-*.png` 与对应 `.txt`。
- 原有 Windows Input 1.5 倍自动验收及 Password 1.5 倍窗口流程再次通过。Gallery catalog 只更新本地 support overlay，保留 Ant Design 6.6.5 固定源码基线。
- 清空期间的组合输入由平台通用测试的模拟会话验证；人工 Windows 中文 IME 候选窗仍待独立验收。Linux 原生验证依用户安排暂缓。
