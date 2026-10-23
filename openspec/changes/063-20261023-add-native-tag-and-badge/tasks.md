# Tasks

## 1. 规划与来源

- [x] 1.1 固定 6.6.5 本地源码核对记录和首版范围；通过 openspec doctor、全量严格规范校验及 git diff --check。

## 2. 平台通用实现（Windows MSVC headless）

- [x] 2.1 实现 Tag/CheckableTag、Badge 与主题 Token、局部窗口注册和组件文档；通过专用 public API、theme、component tests 的 Debug/Release 构建及 CTest，验证响应、保留身份、颜色不测量、布局、指针/键盘/焦点、销毁和空闲。

## 3. 集成检查

- [ ] 3.1 通过格式检查、openspec doctor、全量严格规范校验、git diff --check 与相关既有窗口/主题合同回归，记录证据后英文提交并推送、核对远端 SHA。

Gallery 与综合目录由父任务处理。Linux/Wayland、真实 GPU 人工视觉、系统 IME 不在本次已验证范围。
