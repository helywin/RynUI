# Spec Delta

## Purpose

为 RynUI 使用者提供已实现常用组件的实际交互入口，使状态、反馈、抽屉和确认流程可以在同一个 Gallery 中直接操作，并通过保留内容与受控写回的示例说明公开 API 的用法及支持边界。

## ADDED Requirements

### Requirement: Interactive common component samples

Gallery SHALL 展示 Drawer 四方向、Popconfirm、Tag/CheckableTag、计数/圆点/状态 Badge、Alert、Spin 和 line/circle/dashboard Progress 的实际组件。

#### Scenario: State controls update samples
- **WHEN** 使用者操作计数、选择、关闭、加载或进度入口
- **THEN** 对应组件响应式更新，并保留无关示例与插槽内容

### Requirement: Retained overlay workflows

Gallery SHALL 支持滚动页面后的 Drawer 编辑、Popconfirm 确认取消和关闭重开流程，保持正文草稿与受控状态。

#### Scenario: Reopen after editing
- **WHEN** 使用者打开 Drawer、编辑正文、关闭并再次打开
- **THEN** 编辑内容和组件身份保留，焦点返回有效入口，正文可独立滚动且背景不响应模态输入

### Requirement: Evidence based catalog status

Gallery catalog 和文档 SHALL 列出实际首版支持范围与未实现功能，不把示例集成或 Recording 结果描述为原生平台视觉验收。

#### Scenario: Browse supported components
- **WHEN** 使用者查看这些组件的参考信息
- **THEN** 能找到实际示例与 API 文档入口，支持范围和缺失能力与实现一致
