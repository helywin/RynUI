# Design

## Context

文本记录按声明顺序排列，变长替换时 `remap_following` 会移动后续 primitive 的 range。当前 `synchronize` 在每次替换后调用 `rebuild_ordered_scene()`，它清空并遍历全部 `ordered_ids_`。029 的长距离滚动会在一个宿主轮次里实现化多个新进入视口的文本，导致多次全量重建。临时 Gallery 阶段诊断仅用于定位，未作为正式优化结果。

## Goals / Non-Goals

**Goals:** 同一文本宿主轮次多次替换仅在结束时重建一次 ordered scene；最终 painter order、atlas page 与 instance range 和逐条同步一致；失败/异常后的下次同步可恢复。Windows MSVC Debug 测试与 Release 真实 D3D12 验收。

**Non-Goals:** 不改变 primitive 物理存储与 `remap_following` 的复杂度，不跳过新可见文本 raster，不引入通用 scene graph 批次，不声称 GPU 时间变化。

## Decisions

1. **内部批次边界。** `TextSceneService` 提供 begin/finish/cancel ordered scene batch。批次期间记录 `ordered_scene_pending`，并继续同步各记录的 primitive 和 range；宿主必须在读取 `ordered_scene()` 或同步 fragment 前结束批次。单记录调用在批次外立即重建，维持现有接口合同。拒绝嵌套批次。
2. **失败恢复。** 宿主在循环提前返回时先 finish，保证已经成功的记录有一致 ordered scene；异常时 cancel 保留 pending 标志，下一次正常同步或批次 finish 重建。finish 自身若抛出异常，cancel 关闭批次并保留 pending，不能将过期 ordered scene 当成已确认结果。下一次正常入口即使无记录变化也须处理 pending。
3. **验收与参照。** 测试多个不同长度的文本同轮同步，比较批次和逐条同步的 ordered draw commands 与 primitive range；检查重建次数 1 对多次、零变更无需重建、失败取消后恢复。Gallery 在正式 `windows-msvc` / Ninja Multi-Config / MSVC Release `--clean-first` 构建后五进程复测，比较 029 首帧、滚动平均/p95/max、raster 数、终态及输入路径。最长帧收益必须超过五进程波动；若未改善，记录原因并不声称优化成功。

## Risks / Trade-offs

- 批次期间 ordered scene 暂时落后于 primitive，因此边界只包住单线程宿主同步循环，不暴露给绘制或事件回调。
- `remap_following` 仍逐记录更新；若它而非 ordered scene 重建主导耗时，本 change 的收益可能小于预期。
- 首帧也包含大量首次文本实现化，可能获益；仍需五进程独立测量，不能由源码循环次数推断百分比。

## Migration Plan

按规划、平台通用实现与测试、Windows 窗口复测、集成结果分阶段提交，英文规范提交格式。OpenSpec CLI 1.4.1 不支持 `doctor`；全仓已有独立失败项需如实保留。Linux 平台通用逻辑无需重复验收，Linux 真窗口不由 Windows 结果代替。
