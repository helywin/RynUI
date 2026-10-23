# Spec Delta

## Purpose

为桌面应用提供原生标签和徽标，使分类、可撤销的关闭请求、受控筛选选择、未读数量和业务状态能直接组合既有内容，并在响应式更新时保留内容及交互身份。

## ADDED Requirements

### Requirement: Retained Tag
Tag SHALL 提供 typed content/icon slots、预设/状态/自定义颜色、bordered、disabled、受控 visible 和 onClose；CheckableTag SHALL 通过 checked/onChange 进行受控选择，支持指针和键盘。

#### Scenario: Controlled activation
- **WHEN** 用户关闭或切换标签
- **THEN** 仅产生关闭请求或下一 checked 值，调用者写回状态前原状态保持；禁用时不激活；隐藏时清理焦点和命中。

#### Scenario: Stable content
- **WHEN** 颜色、checked、bordered 或 visible 更新
- **THEN** 内容/icon slot 不重跑，重新显示恢复原内容身份，静态标签不增加 Tab stop。

### Requirement: Retained Badge
Badge SHALL 支持非负整数 count、默认 99 的 overflowCount、showZero、dot、五种 status、text、logical offset、大小和可选 retained child；徽标自身 SHALL 不截获 child 的输入。

#### Scenario: Counter precedence and placement
- **WHEN** count 超限或归零、dot/status/text/offset 更新
- **THEN** 显示上限加号、按 showZero 隐藏零、按 dot 隐藏数字；无有效计数时 status 优先显示状态点，包裹模式定位于 child 右上角且不改变其布局尺寸。

### Requirement: Theme and lifecycle
两组件 SHALL 使用 Theme/Component Token、既有 logical scene 和外部 LayoutStyle；纯颜色更新 SHALL 不触发测量，销毁 SHALL 释放资源与订阅。

#### Scenario: Local theme updates
- **WHEN** Default/Dark/Compact 或组件 Token 更新
- **THEN** 只更新相关颜色或布局，普通属性不重挂载内容，空闲不持续请求帧，CPU scene v3/GPU ABI v2 保持不变。
