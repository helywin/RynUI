# Tasks

## 1. 平台通用实现与合同

- [x] 1.1 实现 typed API、model、Theme、保留式 host、焦点/Modal/子层行为和 docs/popconfirm.md；以 windows-msvc-headless / MSVC Debug 的 Popconfirm 聚焦测试及相关 Modal/Select/Tooltip 回归验证。
- [ ] 1.2 运行格式检查、openspec doctor/strict validate、git diff --check，记录实际证据并提交推送、核对远端 SHA。

## 2. Windows 原生验收

- [ ] 2.1 在真实 Windows 窗口验证显示/按钮/键盘、窗口边缘、DPI 和嵌套 Modal/Select；记录实际证据。当前子任务仅聚焦 Debug 合同，此项由后续原生集成验收完成。

## 3. Linux 原生验收

- [ ] 3.1 在真实 Linux 窗口验证显示/按钮/键盘、窗口边缘和嵌套 Modal/Select；保留 pending，不以 Windows fixture 结果代替。
