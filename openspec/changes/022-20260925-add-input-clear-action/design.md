# Design

## Context

Input 的值保存在窗口级 `TextEditorStore`，输入框通过 typed suffix 挂载内容；Password 已消费 `InputAffixAction`。本变更基于 proposal.md 与 `specs/input-clear-action/spec.md`，不新增编辑会话。

## Goals / Non-Goals

**Goals:** 同一 Input 编辑器、焦点与资源身份上实现清空；自定义 suffix 可以与清空动作共存；空值/状态变化只更新必要的布局、文字和 hit-test。

**Non-Goals:** 重新设计 Input 外观、提供通用图标库、实现 Search 专属 clear 或 InputNumber 数值行为。

## Decisions

1. **复用 Input 自身 editor。** 清空动作在 `InputComponentHost` 中调用现有编辑操作和 `notify_change`，避免另建值桥接或绕过 undo/redo。活动组合输入先由 session 取消。替代方案是向外部调用方暴露 editor；这会泄漏窗口资源与受控回写细节。
2. **扩展内部 `InputAffixAction` 的可见性。** 响应式可见性同时改变标签内容、左右填充和 interaction eligible；组件 ID 与 scene fragment 保持稳定。替代方案是每次值变化 remount suffix，会破坏稳定拓扑。
3. **Input 组合 suffix。** `allowClear` 被显式设置时挂载内部动作，后接调用方的 typed suffix。Input layout 的 suffix 占位由用户 suffix 或当前可见清空动作决定；无内容时不留下空白间距。
4. **主题语义样式。** 清空标签沿用 Input 的语义前景与 typography；不新增独立 Token。组件布局高度/颜色仍由现有 Input Token 控制。

## Risks / Trade-offs

- [清空回调同步销毁 Input] → 在调用前复制值、ID 与回调，之后不访问旧状态。
- [可见性更新引发布局多余失效] → 仅在布尔状态改变时更新 suffix 占位和 hit-test；颜色变化只走现有材质路径。
- [平台字体对清空符号的覆盖差异] → Windows 真实窗口检查系统字体；Linux 依用户要求后置。
