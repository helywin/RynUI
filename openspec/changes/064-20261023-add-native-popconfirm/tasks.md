# Tasks

## 1. 平台通用实现与合同

- [x] 1.1 实现 typed API、model、Theme、保留式 host、焦点/Modal/子层行为和 docs/popconfirm.md；以 windows-msvc-headless / MSVC Debug 的 Popconfirm 聚焦测试及相关 Modal/Select/Tooltip 回归验证。
- [x] 1.2 运行格式检查、openspec doctor/strict validate、git diff --check，记录实际证据并提交推送、核对远端 SHA。

## 2. Windows 原生验收

- [ ] 2.1 在真实 Windows 窗口通过原生自动集成验证显示、按钮、键盘、窗口边缘、DPI 和嵌套 Modal/Select，记录实际自动验收证据；当前 Debug 合同不替代本项。
- [x] 2.1a 历史人工验收已关闭：Windows Popconfirm 的目视与手工交互检查；不要求人工补充显示、按钮、键盘、窗口边缘、DPI 和嵌套 Modal/Select 结果。

## 3. Linux 原生验收

- [ ] 3.1 在真实 Linux 窗口通过原生自动集成验证显示、按钮、键盘、窗口边缘和嵌套 Modal/Select，记录实际自动验收证据；不以 Windows fixture 结果代替。
- [x] 3.1a 历史人工验收已关闭：Linux Popconfirm 的目视与手工交互检查；不要求人工补充显示、按钮、键盘、窗口边缘和嵌套 Modal/Select 结果。

2026-11-03：依用户明确指示，本文件标注“历史人工验收已关闭”的勾选表示取消人工任务门槛，非新增实测通过；混合项中的实现、构建、CTest 和平台自动集成仍独立追踪。后续人工体验建议仅写验收说明，不再新增任务，规则见 [人工检查与任务清单](../../../docs/development/openspec.md#人工检查与任务清单)。
