# Spec Delta

## Purpose

提供靠近操作入口的原生交互气泡卡片，允许用户在保留的富内容中编辑文本、选择值和激活按钮，并在响应属性、主题、输入边界及有限视口变化时保持一致的身份、草稿和输入归属。

## ADDED Requirements

### Requirement: Typed retained content

Popover SHALL 提供 typed trigger/title/content slots 和字符串 title/content Props。普通开闭、属性和主题更新 MUST 保留子组件、编辑草稿和 refs，不重新执行 slots；同一字段的字符串与 slot 以及 open/defaultOpen 冲突 MUST 拒绝。

#### Scenario: Retained editor and button
- **WHEN** 用户在内容 Input 中编辑、关闭并重新打开，再激活内容 Button
- **THEN** 草稿和 refs 保持，Button 原生点击和 Enter/Space 回调可用，slots 各执行一次

### Requirement: Open authority and triggers

Popover SHALL 支持 open/defaultOpen/onOpenChange、disabled、hover/focus/click 的组合及有限进出延时。默认 hover、顶部、有箭头、自动调整，空 title/content 不显示。受控普通关闭请求 MUST 尊重 open authority；Escape、外部点击和原生触发器激活 SHALL 可用且不重复激活子控件。

#### Scenario: Hover and focus continuity
- **WHEN** 指针从入口移动到气泡或子浮层，或焦点从入口进入富内容
- **THEN** 气泡在有效触发原因存在时保持，延时取消与到期准确，Escape 可以请求关闭

#### Scenario: Controlled rejection
- **WHEN** 无焦点的受控 open=true 气泡拒绝 Escape/外部关闭
- **THEN** 普通关闭保持显示，后续强制 suppression 仍生效且不被通知去重跳过

### Requirement: Input ownership and lifecycle

Popover SHALL 保留子 Input/Button/Select/DatePicker/TimePicker/Table 的原生输入和最近 popup 的 wheel 归属，先交给内容处理再做边界 fallback，覆盖的 sibling 不穿透。编辑器和子浮层 SHALL 优先处理 Escape。Modal/Spin/隐藏/失活 MUST 强制抑制陈旧 open 并清理自身输入状态，不能清除 foreign owner 的 block。用户回调、焦点和 authority getter 同步卸载或重绑 MUST 不访问过期 generation。

#### Scenario: Nested popup and suppression
- **WHEN** 子选择器已展开后父层被隐藏、Spin 阻断或 Modal 排除
- **THEN** 父子输入停止、焦点合法化，恢复边界不会复活陈旧弹层；显式新打开可以恢复

#### Scenario: Callback unmount
- **WHEN** onOpenChange、原生 onClick、onBlur 或激活 getter 同步卸载组件
- **THEN** 当前分发安全结束，refs、timer、pointer capture 与 owned block 清理完整

### Requirement: Viewport geometry and independent theme

Popover SHALL 支持12方位、arrow、pointAtCenter、autoAdjustOverflow，有限视口定位、实际 glyph/quad/hit clips 一致，focus decoration 遵守有效外部 clip 并保留可见外扩。视觉 SHALL 仅由独立 Theme 控制并支持继承/reset、独立 seed/algorithm、五组失效、identity 与诊断序列化，旧主题值和 identity 顺序 MUST 保留。

#### Scenario: Small viewport and theme change
- **WHEN** 窗口变窄、关闭 overflow 调整或热更新气泡颜色/尺寸/字体/阴影/顺序
- **THEN** 渲染和命中遵守真实 clip，内容身份保留，只有对应主题订阅失效，旧组件 token 值保持不变
