# Spec Delta

## Purpose

为桌面应用提供有交互能力的原生命令下拉菜单，复用导航菜单的键盘与命令语义，确保右键定位、模态输入限制、焦点恢复及生命周期符合保留式窗口运行时。

## ADDED Requirements

### Requirement: Trigger and controlled visibility

Dropdown SHALL 提供 typed trigger slot、click/context-menu、open/defaultOpen 与开关请求；内嵌 Button 的键盘激活 SHALL 同时执行原有 onClick，不得被窗口 command filter 吞掉。

#### Scenario: Trigger Button
- **WHEN** 用户点击或通过 Enter/Space 激活触发 Button
- **THEN** 原有回调执行一次且 Dropdown 发出一次开关请求

#### Scenario: Controlled refusal
- **WHEN** 外部状态拒绝打开或关闭
- **THEN** 发出请求且实际可见状态服从受控值

### Requirement: Placement focus and boundaries

Dropdown SHALL 使用窗口 clip 翻转/钳制位置，支持右键坐标锚点、外部/Escape/命令关闭和合适的 trigger 焦点恢复；初次隐藏 Modal 内的默认打开 SHALL 在边界激活后显示，曾被环境关闭的菜单 SHALL 不随父层恢复而复活。

#### Scenario: Modal initial opening
- **WHEN** 菜单初始挂载于关闭的 Modal 后该 Modal 打开
- **THEN** 菜单重新计算可见性，场景和命中使用窗口边界

#### Scenario: Nested popup and outside closing
- **WHEN** 用户操作所属子 popup 或在外部点击
- **THEN** 子 popup 操作不误关父菜单，外部关闭不抢回外部焦点，Escape 从最深层开始恢复 trigger 焦点

### Requirement: Retained resource lifetime

Dropdown SHALL 保留关闭菜单的资源以供复用，禁用/窗口失活/卸载 SHALL 撤销可交互状态，回调可安全卸载本组件。

#### Scenario: Callback destruction
- **WHEN** 菜单命令回调卸载 Dropdown
- **THEN** 不访问旧状态且释放全部所属资源
