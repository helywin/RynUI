# Proposal

## Why

审计 B3 的 TUNE-PERF-005/006 指出高扇出订阅与离散 effect dirty 合并的扩展性风险，但尚无本基线实测可确认热点。先提供真实更新、可重复输出与合同回归输入，再依据测量证据决定最小内部改造。

## What Changes

- 新增 Reactive 高扇出测量：K=1/64/256/1024/4096，分别覆盖稳定依赖与动态切换；分开冷挂载、预热、热更新，记录实际写入、回调、耗时与分配。
- 新增 RoundedEffect 离散区间测量：稳定存量 1k/10k，geometry/material 分别更新并覆盖两者的离散并集；通过共同 SceneResources 事务记录同步耗时、真实上传区间/字节及 packed 内容。
- 输出每次样本与每轮汇总 CSV，支持可控 iterations、warmup 和 rounds；至少三轮真实 baseline 由父线程集中执行，完整保留结果。
- 测量后才决定是否保留依赖/稳定订阅句柄，以及批量 dirty 合并的最小改造；保持异常、销毁、动态依赖、重入、事务失败和 epoch 合同。源码准备是第一阶段，父线程按正式构建、基线、最小改造及独立复核依次验收。

## Capabilities

### New Capabilities

- `reactive-effect-scaling-measurement`：高扇出响应与离散 effect 上传的确定性输入、可核验工作量、测量输出及先测后改证据合同。

### Modified Capabilities

无；不改变公开 Reactive、renderer 或组件 API。

## Impact

- 首阶段仅新增两个 tests benchmark、必要的最小测试配套、tests/CMakeLists.txt 注册及本 change 规划/交接。基线确认热点后，生产改动限于 Reactive 私有订阅存储与共同 effect dirty 合并；不修改 101/102 或组件业务实现。
- 不涉及 Tree/Table/Gallery、Theme、golden、资源、字体、SDL 窗口或平台线程。Reactive 目标只链接 Core Reactive；effect 目标使用 renderer/common 与 Recording，维持单向依赖。
- 风险：无扫描/排序迭代私有计数，时间与分配只能提供可观测证据；Recording 的复制成本包含在同步中，不能当作真实 GPU 性能；1k 实例无法形成 1k 个互不相邻的非空单实例区间，具体有效矩阵在 design 中定义。
- 可验证结果是输入确实变化、每个 observer 输出正确、真实上传区间与字节一致、失败可重试、输出可重复；没有任何耗时阈值证明算法错误。Windows MSVC/Ninja Multi-Config HEADLESS Debug/Release 及三轮测量待父线程执行，不声称热点或改善已确认。
