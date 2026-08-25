# Reactive runtime validation

验证日期：2026-08-26

`rynui_reactive_allocation_benchmark` 在预热后的 `Signal<int> -> Memo<int> -> Effect` 已挂载图上连续执行 10,000 次不同值写入。分配计数仅包围 `Signal::set()` 及其同步 flush 路径；计时不作为性能门槛。

| Preset | Configuration | Writes | Heap allocations | Effect runs | Result |
| --- | --- | ---: | ---: | ---: | --- |
| `windows-msvc` | Debug | 10,000 | 0 | 10,257 | PASS |
| `windows-msvc` | Release | 10,000 | 0 | 10,257 | PASS |

Debug 与 Release 的完整 CTest 均为 17/17 通过，其中 reactive 测试覆盖 `Signal`、`Memo`、`batch()`、`Scope`/Effect 和 steady-state allocation。
