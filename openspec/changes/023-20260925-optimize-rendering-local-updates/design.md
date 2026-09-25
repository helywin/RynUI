# Design

## Context

`DirtyQueues` 当前用 vector 线性查重；`QuadInstanceStore` 与 `GlyphInstanceStore` 每次标脏就排序合并；`GlyphAtlas` 在 deque 中线性查找 key。三者保留局部更新语义，却让收集阶段随已累计的工作或 atlas 条目增长。现有 GPU buffer 同步和测试依赖按序的脏区读取，atlas entry 地址依赖 deque 稳定性。研究文档指出应先建立测量边界，再独立消除这些 CPU 成本。

## Goals / Non-Goals

**Goals:** 在不改变公开 API、视觉结果和 GPU buffer 合同的前提下，使 dirty 入队按 slot 容量摊销为常数工作；一次读取才整理 Quad/Glyph 脏区；atlas 查找按完整 key 索引；留下可复现的规模证据。

**Non-Goals:** 本 change 不承诺整帧 FPS、GPU 执行时间或输入到展示延迟收益；不改变 SDL cycling、目标 buffer 生命周期、文本连续 span、共享滚动变换或大数据虚拟化。这些依赖后续独立设计与真实设备实验。

## Decisions

1. **每 domain 的 slot stamp。** 各 domain 保存 `slot -> (generation, epoch)`；dense vector 保留首次入队顺序。`clear()` 前进 epoch，回绕时清空 stamp。这样不对每次入队扫描旧队列，也避免 `unordered_set` 的逐元素分配。自动布局根仍按现有父链确定；高树深的根查找另列为后续布局阶段问题。
2. **读取边界归并脏区。** Store 在 `mark_dirty` 时仅 append；dirty getter 首次调用时对该 domain 原位排序合并，重复读取直接复用。变长 replace 先裁掉受影响后缀的旧区间，再标记新的完整后缀。相邻范围归并不改变上传 byte 覆盖。保留既有 getter 与 GPU buffer 同步 API，避免将未排序范围交给旧调用方。
3. **Atlas 使用 key 索引到稳定 deque 下标。** 自定义完整 key hash 的 `unordered_map` 只保存下标；entry 保留在 deque 中。查找先 hash 再比较完整 key。仅在成功建立 entry 后加入索引；bitmap 无效、过大或容量错误不缓存为成功。页级淘汰与 UV generation 尚未引入，避免提前改变 residency 合同。
4. **规模基准同时报告工作量与时间。** 保留相同 seed、场景大小和变更量，对旧提交及新提交运行相同基准。记录墙钟 CPU 时间、操作/区间计数和运行环境；真实 GPU 未参与的结果标为未测。Windows 使用 `windows-msvc` 的 Ninja Multi-Config Debug 验证和 Release 测时；Linux 平台通用逻辑无需重复，但涉及 Linux 后端的未来变更必须原机验收。

## Risks / Trade-offs

- [stamp 随最大 slot 扩容，内存与历史最大节点数相关] → 记录容量与高水位，只有有 dirty 的 domain 扩容；不按每次 frame 释放。
- [slot 重用使队列里存在旧 generation] → 消费侧过滤非 live ID；新 generation 的 stamp 不与旧 generation 混淆。
- [lazy getter 使排序成本转移到调用点] → 同一批更新只整理一次，基准分别计入收集与规划，不能只报告收集时间。
- [hash index 增加 CPU 内存] → 记录 entry/index 规模，维持既有 atlas 页上限；真实字体与 CJK 回归校验 key。
- [全局主题更新依旧需要 O(D) 与全量绘制] → 只承诺去除与累计已入队量有关的重复扫描。

## Migration Plan

先添加基准与针对 generation、乱序区间和 atlas 错误的回归，记录旧版 CPU 基线；随后分别改 queue、脏区、atlas，每阶段运行定向 CTest 并英文提交。每项仅修改内部数据结构，可按提交独立回退。最后运行正式 preset 的受影响测试、OpenSpec 和差异校验，记录实际平台覆盖。

## Open Questions

真实 SDL transfer 资源成本与全帧热点排序仍需后续 P0 GPU/窗口测量；本 change 的 CPU 基准只决定这些局部算法的效果，不据此选择 P1b–P4 的参数。
