# Proposal

## Why

现有 Input 已共享窗口级编辑、IME 与剪贴板服务，但直接显示原文，无法安全呈现密码。Password 可以检验文本展示策略是否能复用 Input 编辑模型，而不复制第二套编辑宿主。

## What Changes

- 增加公开 `Password` typed Props，支持受控/默认值、隐藏展示、可见性切换、禁用与只读状态，以及 Input 的尺寸、状态和布局语义。
- 为 Input 展示层增加按字素遮罩和双向光标映射；隐藏时场景文本不含原文，编辑回调仍提供原值。
- 增加保留编辑焦点的切换控件，并向平台文本输入端口传递密码输入类型；隐藏状态禁止复制和剪切原文，允许粘贴。
- 增加平台通用回归及 Windows 原生窗口验证；Linux 专属验证依用户安排暂缓。

## Capabilities

### New Capabilities

- `password-input`: 密码编辑、遮罩展示、可见性、剪贴板和焦点合同。

### Modified Capabilities

无。

## Impact

新增 `include/ryn/password.hpp`，扩展 Input 展示投影、交互焦点策略和 SDL 文本输入类型，复用现有窗口编辑服务、Input 场景与 Theme Token。首版不提供强度校验、密码管理器集成、自定义切换图标或 hover 切换。主要风险是组合输入的字节映射、切换时 IME 会话稳定性与隐藏文本泄漏。
