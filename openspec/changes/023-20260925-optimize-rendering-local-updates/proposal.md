# Proposal

## Why

现有局部更新仍在 dirty queue 去重、primitive 脏区整理和 glyph atlas 查找中反复扫描已收集的数据。研究文档 `docs/research/ui-rendering-scene-performance.md` 将这些确定的扩展成本列为优先改造点；先消除这些成本，才能准确评估后续场景和 GPU 改造。

## What Changes

- 增加可重复的局部更新工作量基线，记录队列、脏区和 atlas 的规模与操作量，并保留运行环境和测量边界。
- 将各 dirty domain 的重复入队改为 generation-aware 的稀疏去重，同时保持首次入队顺序、显式 subtree 根语义和清空后的再次入队能力。
- Quad/Glyph 实例更新先累积脏区，在消费边界一次排序、合并；保持实际上传范围精确，不扩大稀疏更新。
- Glyph atlas 增加完整 key 的索引；保留稳定 entry 地址、页分配、失败及 dirty region 合同。
- 为上述行为增加正确性和规模回归测试，使用正式 Windows MSVC preset 验证。

本 change 聚焦研究路线的 P0 局部工作量观测和 P1a。GPU 提交事务、staging、稳定 span、共享 transform、虚拟化和 damage 依赖后续独立 change 与真实瓶颈数据，不在此 change 中宣称完成。

## Capabilities

### New Capabilities

- `rendering-local-update-efficiency`: 渲染局部更新的去重、脏区规划、atlas 查找与规模观测合同。

### Modified Capabilities

无。

## Impact

涉及 `src/runtime/invalidation.*`、`src/graphics/{quad_primitive,glyph_scene,glyph_atlas}.*`、相关测试和 benchmark。公开 C++ API、绘制顺序、视觉输出和依赖锁不变。风险集中于复用 slot 的旧 generation、脏区只在消费时归并后的调用方预期，以及 atlas 插入失败后的索引一致性；使用定向回归和现有 CTest 验证。Windows 真实 GPU 延迟与 Linux 原生窗口效果需要在依赖相应后端的后续阶段独立验收。
