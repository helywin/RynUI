# Proposal

## Why

Input 已有 typed suffix 和窗口编辑服务，但常见的清空操作仍需调用方自行拼接交互、处理受控值及焦点。新增 `allowClear` 可以验证 `InputAffixAction` 在 Password 之外的复用，并提供基础输入组件的常用能力。

## What Changes

- `InputProps` 增加 reactive `allowClear`，有内容且可编辑时显示清空操作；保留已有自定义 suffix。
- 清空操作复用内部 `InputAffixAction`，支持指针和键盘激活，不夺走 Input 的指针焦点；通过现有 editor 和 `onChange` 交付空值。
- 空值、禁用、只读或关闭 `allowClear` 时折叠操作与命中区，稳定组件结构；值、状态和主题更新保持最小失效范围。

## Capabilities

### New Capabilities

- `input-clear-action`: Input 的响应式清空操作、编辑回调、焦点及资源合同。

### Modified Capabilities

无。

## Impact

涉及 `include/ryn/input.hpp`、Input 实现、内部 `InputAffixAction` 与测试/Gallery。无第三方依赖。需验证受控回写、IME 组合输入取消、清空时的可见性和布局、销毁清理及真实 Win32 渲染。Linux 原生验证依用户安排暂缓。
