# Design

## Context

见 `proposal.md`。现有 `HitTestSnapshot::refresh` 为每条记录对 `dirty_nodes` 做 `any_of`，每次比较调用 `node_descends_from` 沿父链检查；`refresh_record` 再按 paint 顺序重建受影响记录。`NodeId` 已有 slot index 与 generation，`NodeStore` 提供 slot capacity。Gallery 的 `frame_layout_us` 包含文本、participant、effect、fragment 和命中同步，尚不能把整个 9.4 ms 归因于命中刷新。

## Goals / Non-Goals

**Goals:** 用定向基准确认批量刷新成本；在批量路径把选择受影响记录的工作从逐记录扫描整批 dirty ID 改成按 slot 的本轮标记与每记录一次祖先链查找；保留 refresh 次序和结果。使用 Gallery 真实 D3D12 窗口测量端到端 CPU 阶段，区分定向收益与整帧收益。

**Non-Goals:** 不改变命中点查询、`rebuild`、交互声明顺序、GPU 上传或滚动坐标合同；不声称 `frame_layout_us` 是纯布局时间或 GPU 执行时间。

## Decisions

1. **小批量继续线性路径。** dirty 数不超过固定小阈值时复用现有算法，无新 scratch 分配。备选方案是每次都建 hash set；它对单控件更新增加分配及 hash 工作。
2. **大批量使用持久 slot stamp。** 每个 slot 保存 dirty generation 与本轮 epoch。按 `NodeStore::slot_capacity()` 扩容一次并复用；标记所有有效、在容量内的 dirty ID，记录逐祖先检查 stamp。比较完整 generation，且在 `nodes.find` 之前比较当前 NodeId，与旧算法对已销毁记录的行为一致。epoch 回绕时清零 stamp。备选方案是 `unordered_set<NodeId>`，但会对每个 dirty 节点分配并增加缓存不确定性。
3. **保留 paint 顺序刷新。** 仍按 `records_` 顺序调用 `refresh_record`，这样父交互先于子交互，eligibility 与 clip 传递不变。只替换影响判断，不重写快照布局。
4. **先测局部，再看整帧。** 在优化前添加固定 seed 的 parentless 与嵌套场景，覆盖少量和批量 dirty。Windows MSVC Release 各运行五个独立进程。真实 Gallery `--scroll-acceptance` 再运行五次，记录 CPU p95/阶段、节点与交互规模、实际 D3D12/DXIL、上传与 draw 数；GPU 执行仍标未测。

## Risks / Trade-offs

- [stamp 占用与历史最大 Node slot 数相关] → 仅大批量首次使用时扩容，后续复用；记录容量。
- [slot 重用或 epoch 回绕产生假命中] → stamp 同时比较 generation 与 epoch，并以回绕及复用测试覆盖。
- [改变影响选择后遗漏祖先 clip/eligibility 更新] → 与逐节点参考结果、命中点和 `records_refreshed` 对照，保持原有刷新顺序。
- [Gallery 整帧变化被 vsync、字体或系统噪声掩盖] → 固定机器、preset 和场景，报告五次原始值与中位数；不把局部基准乘数外推到帧率。

## Migration Plan

先提交规划，再加入 benchmark 并记录旧实现基线；随后实现 stamp 和等价性测试，在 Windows MSVC Debug 定向通过后提交；最后 Windows MSVC Release 干净构建、重测 benchmark 与 Gallery、运行受影响 CTest 和 OpenSpec 验证，各阶段单独英文提交。可按提交回退，公开 API 无迁移。
