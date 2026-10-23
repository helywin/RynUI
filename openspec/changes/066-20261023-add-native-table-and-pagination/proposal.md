# Proposal

## Why

业务数据界面需要可实际使用的原生表格与分页。当前已有 Checkbox、Select、Tabs、Card 及 retained scene 服务，但缺少按稳定行身份更新的 Table 与可组合 Pagination。

## What Changes

- 增加 typed Pagination：受控/非受控页码、总数、每页数量、前后页、有限页码窗口、禁用及尺寸。
- 增加 `Table<Row>`：响应数据源、必需稳定 rowKey、挂载时固定 typed 列、响应表头/单元格 slot、宽度/对齐、边框、空/加载状态。
- 增加受控单列排序、Checkbox 多选与回调、有限双轴 viewport 滚动；分页通过共享 current/pageSize 可组合。
- 保留相同 key 的行 Signal 与单元格作用域；普通值变化不重跑 slot。增删行只挂载/销毁相应行。
- 首版不含虚拟化、服务端管线、固定列、复杂 span、树、编辑器或自动列 schema diff。Gallery/catalog/README 由父任务整合。

## Capabilities

### New Capabilities

- `native-pagination`: 原生分页与受控事件。
- `native-table`: typed retained 数据表、排序、多选、滚动及主题。

### Modified Capabilities

无。

## Impact

新增公开头、组件 host、独立测试与使用说明；仅在 Theme、CMake、WindowComponentServices 和公开聚合头添加必要接线。不改变 CPU scene v3/GPU ABI v2，不引入 SDL 或依赖。风险集中于响应更新与销毁重入、行身份及滚动 clip，通过 headless 场景和实际回调测试验证；不以这些结果代替 GPU/Linux/人工验收。
