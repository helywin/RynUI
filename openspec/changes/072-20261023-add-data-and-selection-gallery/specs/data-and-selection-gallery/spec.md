# Spec Delta

## Purpose

为 RynUI 使用者提供实际数据驱动的表格、分页、菜单和数值输入示例，通过可操作的选择、命令和编辑流程说明受控状态写回及保留内容的行为，便于直接组合到桌面业务页面。

## ADDED Requirements

### Requirement: Retained data browsing samples

Gallery SHALL 展示可操作的 Table 与 Pagination，支持排序、选择、翻页、单元格编辑和有限数据更新，并保留相同 key 对应的内容。

#### Scenario: Edit and browse rows
- **WHEN** 使用者在滚动页面后编辑数据，再排序、翻页或更新同 key 记录
- **THEN** 显示反映受控数据，保留仍存在的行组件和有效编辑内容，分页与表格保持一致

### Requirement: Menu commands and numeric input samples

Gallery SHALL 展示 Inline Menu、Dropdown 命令和 InputNumber 的受控选择与数值行为。

#### Scenario: Apply commands and numeric changes
- **WHEN** 使用者以指针或键盘选择菜单命令、调整或输入数值
- **THEN** 对应状态和反馈更新，禁用/只读输入遵循自身约束，关闭浮层后有效焦点恢复

### Requirement: Accurate catalog and retained gallery

目录与文档 SHALL 反映首版支持范围；普通状态和主题变化 SHALL 保留无关示例。

#### Scenario: Inspect and update samples
- **WHEN** 查看组件参考信息或切换示例状态和主题
- **THEN** 能找到实际 API 和交互入口，支持缺口明确，既有样例身份及行为保持
