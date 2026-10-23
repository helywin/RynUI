# Tasks

## 1. 数值合同（平台通用）

- [x] 1.1 新增 typed API 和十进制模型，文档化域/舍入/受控草稿规则；独立 MSVC headless Debug 模型及 public API 测试通过后本地提交。

## 2. Retained 组件（平台通用）

- [x] 2.1 复用 Input/editor/session，接入 retained 按钮、键盘、ref/slots、主题及规范化；组件测试覆盖受控拒绝/回写、空/partial、bounds/step/precision、非法配置原子性、preedit/commit、身份、焦点和销毁，Theme 测试覆盖继承、dirty 分组及旧 golden 字段；更新 docs/input-number.md，Debug 聚焦测试通过后本地提交。

## 3. 集成核验（平台通用）

- [ ] 3.1 在 Windows `windows-msvc-headless` MSVC/Ninja Multi-Config 完成 Debug/Release 聚焦数值、Input 家族回归、Theme golden 和 backend boundary 检查；执行格式、OpenSpec doctor/strict validate、diff check，记录实际命令与限制并本地提交。

本 change 不将真实窗口 GPU、系统 IME 或 Linux 人工验收作为已完成结果；这些工作由父任务后续独立集成。本工作树禁止 push、PR 与 archive。
