# Spec Delta

## Purpose

为单行 Input 提供与现有编辑、焦点和 typed suffix 兼容的清空操作，使使用者能够以响应式属性启用该操作，并通过普通变更回调接收清空结果。

## ADDED Requirements

### Requirement: 响应式清空操作
`Input` SHALL 接受 reactive `allowClear`；仅在允许清空、当前值非空且组件可编辑时显示并启用清空操作。操作隐藏时 SHALL 不保留可点击或可聚焦的命中区，自定义 suffix SHALL 继续存在。

#### Scenario: 有内容且可编辑
- **WHEN** `allowClear` 为 true 且 Input 内容非空、未禁用且非只读
- **THEN** 清空操作可通过指针和 Tab/Enter/Space 激活

#### Scenario: 条件变化
- **WHEN** 内容变空、`allowClear` 变为 false，或 Input 变为禁用或只读
- **THEN** 清空操作及其命中区折叠，其他 suffix 内容保持可用

### Requirement: 清空沿用编辑与受控值合同
清空 SHALL 经由当前 Input editor 修改为零长度值，并且仅在值实际变化时触发一次 `onChange`，传入空字符串。受控输入 SHALL 保持现有受控回写规则，不自行更改外部 `Prop`。

#### Scenario: 指针清空与焦点
- **WHEN** 用户在已聚焦、非空的 Input 上点击清空操作
- **THEN** 内容变空并发出一次空值变更，Input 保持焦点和输入会话

#### Scenario: 组合输入时清空
- **WHEN** 清空操作在活动组合输入期间被激活
- **THEN** 组合输入被取消，编辑值变空，不提交未完成的组合文本

#### Scenario: 受控值回写
- **WHEN** 受控 Input 发出清空变更后外部值再次更新
- **THEN** 显示内容按外部值协调，不重复发出清空回调
