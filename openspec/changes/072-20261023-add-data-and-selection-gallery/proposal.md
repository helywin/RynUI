# Proposal

## Why

Table/Pagination、Menu/Dropdown 与 InputNumber 已完成首版实现及审查，现需实际业务数据和组合用例，便于直接试用。Gallery 应展示数据编辑、分页、选择和命令反馈，而不只提供参考条目。

## What Changes

- 追加有限键控数据 Table 与受控 Pagination，展示排序、选择、编辑及数据更新后的身份保留。
- 追加 Inline Menu、点击/右键 Dropdown 和受控数值输入，用真实状态回写展示交互。
- 更新 support overlay/generated catalog、文档入口和实际 inventory，保留既有样例身份与行为。
- 用 Recording 验证页面滚动后的数据与选择流程，完整 Gallery 回归及原生启动检查独立记录。

## Capabilities

### New Capabilities

- `data-and-selection-gallery`: 数据浏览、菜单命令和数值编辑的实际组合用例。

### Modified Capabilities

无。

## Impact

局部修改 Gallery、目录、测试和文档，不改变公开组件能力、依赖或 renderer ABI。依赖 change066/067/068 的已审查实现；原生视觉/DPI、Linux、人工系统 IME 仍由各组件独立验收。风险为较多保留单元格造成示例规模增长和交互索引变化，使用有限数据与标签定位，并按实际挂载更新断言。
