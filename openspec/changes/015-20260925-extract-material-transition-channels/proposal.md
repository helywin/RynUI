# Proposal

## Why

Button 与 Input 都通过同一个 `AnimationRuntime` 建立 Material/Animation target、owner scope、原地 retarget 和销毁清理，但各自维护一套相似的注册与重定向流程。Switch/Checkbox 又使用同一 Runtime。新增控件时继续复制这些生命周期代码，容易遗漏 target 释放或错误的 dirty domain。

## What Changes

- 提取内部固定通道数的 typed target scope 与材质通道 retarget helper，让 Button/Input 复用 scope、target 注册、异常回滚、销毁和相同目标的收敛规则。
- Button 继续自行解析 Button Token、loading mix 和 spinner phase；Input 继续自行解析 Input Token、选区、光标和 shadow。各自明确声明 Material/Animation dirty domain，不通用化视觉样式。
- 用现有动画、场景、allocation 和 Button/Input 同窗回归验证行为与 scene topology 不变。
- 本 change 仅重构内部机制，不新增公开 API 或用户可观察行为，因此设置 `skip_specs: true`。通用 retained surface 命名和适配另行评估，不在本轮改动。

## Capabilities

### New Capabilities

无；内部复用不新增产品功能。

### Modified Capabilities

无；Button/Input 既有动画、视觉和失效合同不变。

## Impact

- 涉及 `src/animation/`、`src/component/button_component.*`、`src/component/input_material_transition.*` 与对应测试，不改变 `include/ryn/`。
- 013 的 `WindowComponentServices` 继续持有唯一 `AnimationRuntime`；015 只抽取每个组件对该 Runtime 的 target 生命周期和 retarget 样板。
- 不修改 spinner 视觉、Button focus ring、Input active shadow、Token 映射、布局或 editor/IME 服务。Linux 平台验收依用户安排暂缓。
