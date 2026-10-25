# Spec Delta

## Purpose

为 RynUI 窗口内的实际按钮事件提供可挂载、更新与关闭的消息和通知。应用通过显式作用域持有安全引用，通知富内容、输入草稿、焦点和到期回调遵循保留组件生命周期与既有输入边界。

## ADDED Requirements

### Requirement: Window scoped dynamic notice API

系统 SHALL 提供 typed holder、Props、slots 与安全 Ref，在按钮事件内以非空 key 打开 Message/Notification、同 key 更新、close 和 clear；卸载后 Ref SHALL 无效果返回失败。

#### Scenario: Button opens and updates
- **WHEN** 用户点击按钮并连续打开相同 key
- **THEN** 窗口显示一条真实通知且只更新明确提供的字段；不同窗口互不影响

### Requirement: Common visual semantics

系统 SHALL 支持 success/info/warning/error/loading、字符串与 typed 富内容。Message SHALL 顶部居中，Notification SHALL 支持四角堆叠、标题、描述、真实关闭 Button 和可选 action slot。loading SHALL 明确显示静态图标。所有视觉 SHALL 由 Theme 控制并遵循有限视口裁剪。

#### Scenario: Rich notification in finite viewport
- **WHEN** 应用打开包含 Input 和 action Button 的通知并改变窗口大小、Theme 或 placement
- **THEN** 实际控件仍可交互且保留草稿/ref；越界内容被裁剪，open 不抢焦点

### Requirement: Retained keyed content

同 key 更新 SHALL 保留 entry 身份、未明确替换的 slots 与所有无关 entries。明确传入的 slot SHALL 只重建其对应内容区域。maxCount SHALL 移除最早插入的 entry，更新 SHALL 不改变插入顺序。

#### Scenario: Partial keyed update
- **WHEN** 第一条富内容通知持有编辑草稿，应用更新字符串或另一条通知
- **THEN** 第一条 Input、草稿、ref 与 slot 执行次数保持不变

### Requirement: Deadline and reentrant lifecycle

duration=0 SHALL 持久，正 duration SHALL 按非阻塞单调 deadline 到期；Message/Notification 默认分别为 3/4.5 秒，hover/窗口失活 SHALL 继续计时。idle 无 deadline SHALL 不持续请求帧。关闭 SHALL 在销毁 entry 后最多调用一次回调；同步销毁、重绑、重复 key SHALL 不访问旧 generation。

#### Scenario: Callback reopens the same key
- **WHEN** 到期回调打开相同 key 或卸载 holder
- **THEN** 新 entry 不被旧批次关闭，旧 refs/输入/slot/deadline 资源完整清理且无失效访问

### Requirement: Input ownership

notice SHALL 遵守 Modal、Spin 与祖先隐藏边界，关闭 focused 内容 SHALL 安全清理焦点与输入会话，卸载 SHALL 只清理自身 owned blocks。子控件/子 popup SHALL 先处理 wheel/Escape，covered sibling SHALL 不吞事件。

#### Scenario: Covered and suppressed content
- **WHEN** notice 被其他浮层覆盖或位于受阻输入边界外
- **THEN** 不越过边界接受输入，不吞顶层 popup 的 wheel/Escape，清理不改变 foreign owner
