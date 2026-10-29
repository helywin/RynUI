# Proposal

## Why

审查 TUNE-PERF-003/004 指出 Tree hover 进入整树刷新并无条件标记结构变化，Table 选择变化重新复制并排序全部记录。这些局部操作不应随数据规模重复进行结构和排序工作。以 `dd3e71f775fb8d4f1f46aeeff5ea444ba816e54d` 为基线修复实际路径。

## What Changes

- 分离 Tree hover/focus 材质变化与展开、选择、勾选投影，固定拓扑采用索引 membership。
- Table 按实际数据、排序与分页变化更新投影；选择/bordered 更新复用投影，减少 membership 线性扫描。
- 用访问量、结构重建、comparator 和分配计数验证原因，保留完整 generation、草稿、富 slot 和焦点顺序。
- 非目标：Tree 动态拓扑/虚拟化、Table 新功能或双 caret 视觉调整、公开 API/Theme 改动。

## Capabilities

### New Capabilities

- `tree-state-projection`：固定拓扑 Tree 的局部状态更新与结构投影隔离。
- `table-state-projection`：Table 数据/排序/分页投影与选择/材质更新隔离。

### Modified Capabilities

无。当前主 specs 目录为空；保持既有受控交互合同。

## Impact

主要涉及 Tree/Table host/model 及其专项测试和微基准。缓存失效、重入排序回调、disabled conduction 和焦点顺序是重点风险。平台通用验收在 MSVC HEADLESS Debug/Release 执行；不重写 Gallery 或共享 renderer。
