# Design

## Context

动机见 proposal。冻结起点为 `76c469bd5522ee64738183323fafe45c26488904`。`ObserverNode::run` 拆除并重建依赖，source subscribe/unsubscribe 扫 subscribers；`RoundedEffectGpuResources::append_dirty_range` 每次追加都排序/合并。CPU `RoundedEffectStore::mark_dirty` 也存在排序合并，因此必须分开测量 store 更新与 GPU resource 同步，不能混在一起归因。

## Goals / Non-Goals

目标：确定性非等值输入、每次样本及每轮汇总、可核验的 observer 结果与上传字节、明确不可观测项，供至少三轮 baseline 与后续最小改造比较。

非目标：基线之前不改变生产算法；不创建通用 benchmark 框架，不引入第三方依赖，不模拟扫描次数，不把 Recording 或时间门限当成 GPU/性能验收。

## Decisions

### Reactive 测量口径

- 五种 K；每轮每种模式创建新 graph。冷挂载单列，scheduler 的 thread-local capacity 可能从同进程先前 case 保留，故不是冷进程/冷缓存。
- 稳定模式全部 observer 读取同一 Signal 并保存逐 observer 派生值；动态模式读取 selector 与所选 source，逐次执行切换、活动 source 更新、已脱离 source 更新，分开输出。只有最后一种预期没有回调，专门证明解除订阅；不能将它纳入高扇出更新收益。
- 每次操作记录 set 返回成功次数、真实 callback 内计数、实际读取次数、scheduler epoch 差、时长和 allocation_probe 分配次数/请求字节/最大请求。每个 observer 的值与调用次数在计时区外检查。Equal comparator 计数不能代替 subscriber 扫描次数。
- 冷挂载、warmup、hot 分开；默认少量 iterations，可显式提升。CSV 保留每次样本及各 phase/operation 的总量、均值和 nearest-rank p95。异常、source 生命周期、batch/reentry 等复用现有测试，新增高扇出输入附带动态解绑与 scope dispose 验证。

### Effect 数据与事务

- `(N,R)` 为 `(1000,256)`、`(10000,256)`、`(10000,1000)`。选中 `floor(i*N/R)` 的单实例位置，间距至少 2，以固定整数排列顺序更新；每次核验实际 dirty spans。1000 实例不可能容纳 1000 个互不相邻非空区间，故不造无效交叉样本。
- material、geometry、mixed（两组交错）、overlap（同一索引同时改 geometry/material）分别测试。opacity 在两个非零值间切换，geometry 做小幅平移；所有实例保持可见、稳定 identity/order、相同 metrics 与存量，无 compaction/full-upload 混入热样本。
- 先计时/统计 store 更新及首次 geometry/material dirty span 读取，再计时 `SceneResources::synchronize`。首次读取可能触发延迟归一化，必须包含在 update 计时中，不能由区外正确性检查提前消耗；baseline 与 after 保持同口径。采用既有 RecordingRenderer 的测试转发适配器，记录实际 effect upload offset/size，并让 Recording 保存真实 buffer bytes；不另造生产上传路径。
- 使用既有 `SceneUploadTiming`：effects-glyphs 是完整 effect 同步（含 compact 检查、规划、打包和 Recording enqueue），committed-effects 是 commit，外层是完整共同同步。纯 planning 时间不可分离，CSV 留空并在交接注明，不把差值当规划时间。
- 每次同步后核验实际上传区间及字节总数、GPU counters 差值、CPU/GPU packed 内容与未改邻居；校验与输出在计时区外。冷初次上传、warmup、hot 分开。Recording 的字节复制分配属于测得同步成本；转发区间记录容器预先 reserve，避免日志分配污染。
- 失败注入/epoch 的基本合同沿用 `scene_backend_tests.cpp` 与 `rounded_effect_gpu_resources_tests.cpp`，在新增离散输入上补一次部分 upload 失败、commit 失败、retry 和 epoch 恢复检查，不纳入热基准。

### 输出与构建边界

两个独立可执行目标在 HEADLESS return 前注册，Reactive 只链接 `rynui_reactive`，effect 链接 `rynui_renderer_common` 与 `rynui_renderer_recording`。最小共用测试辅助仅负责参数、分配计量和统计，保持单翻译单元 allocation_probe 使用。CTest 只执行小输入的正确性合同，不断言耗时阈值。

平台通用逻辑在父线程的 Windows MSVC/Ninja Multi-Config `windows-msvc-headless` Debug/Release 验证一次；并行编译最多 2 jobs，测量不并发构建。运行至少三轮 Release，记录源码及二进制 hash、preset、机器、工具链、参数与全部 CSV。未新增 OS 专属行为；Linux、真实窗口、GPU/driver、系统字体和 input/DPI 不在此次 HEADLESS 证据范围，不以 Windows 逻辑测试替代。

### 先测后改

首阶段仅交接源码。父线程收集 baseline 后，分别判断热点归因与最小方案；Reactive 可比较保留未变依赖/稳定订阅句柄，effect 可比较先收集再合并/有序归并。没有实证支持时记录暂不改算法的决定；若要更细归因，先增加最小私有 instrumentation 再测，不能从 K² 估算输出“实测 scan”。

2026-10-30 父线程完整三轮 Release baseline 已确认两个扩展性热点，原始数据见 evidence/windows-baseline（阶段记录冻结后提交）。选择如下最小实现方向：

- Reactive 使用可复用、有顺序的 subscription slots 与 dependency handle，避免每次退订/订阅全表扫描；继续在 run 开始解绑、get 时订阅并排到尾部。先固定 A/C/B 重订阅顺序、read 前/后写入、动态解绑、source/Scope 销毁和分配失败合同，不改成回调结束才取消旧依赖。
- effect store 复用现有 DirtyRangeAccumulator；首次 dirty getter 的整理成本已计入基准 update。renderer 对两个已经归一化的 spans 一次有序归并，保留相邻/重叠并集、完整 packed bytes、上传失败重试与设备 epoch。更新需先准备 dirty 元数据再发布实例，避免新失败路径丢失 dirty。
- baseline 的 observer、读写、上传区间/字节及分配为实测；内部扫描次数与纯 planning 仍不可观测。热点选择结合实际规模曲线和源码路径，不把理论复杂度计作实测工作量。后续用同一 harness、同机、同数据及三轮样本比较。

## Risks / Trade-offs

- [时钟和 allocator 干扰] → 保留所有样本，输出实际工作量、配置与 p95；短默认值仅用于方便运行，不等于稳定性能结论。
- [观察范围有限] → subscriber 扫描次数、排序比较次数、纯 planning 时间均不可观测；现有输出足以做初步扩展性 baseline，精确归因另需内部计数。
- [Recording 成本遮挡] → 分列 store/effect sync/transaction commit；保持同机同数据、同 instrumentation 的前后比较。
- [源码交接边界] → 首批 worker 禁止构建/运行，原交接保持 source-only；后续父线程仅按正式验证与 evidence 逐项勾选。

## Migration Plan

源码准备 → 正式构建与合同/三轮 baseline → 有证据才做最小算法改造或记录不改决定 → 同口径回归与独立审查。父线程分阶段提交并立即安全推送；本 worker 不提交、不访问 remote、不 archive。
