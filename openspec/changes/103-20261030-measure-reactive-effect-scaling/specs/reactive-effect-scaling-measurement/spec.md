# Spec Delta

## Purpose

为 RynUI 高扇出响应和离散 effect 上传提供可重复、可核验的测量输出，区分实际执行工作、分配、耗时与尚不可观测的内部成本，支持先取得基线再决定最小改造，并保留结果正确性与失败恢复合同。

## ADDED Requirements

### Requirement: 高扇出响应测量必须执行真实更新

测量工具 SHALL 覆盖 K=1/64/256/1024/4096 的稳定依赖和动态依赖，分开冷挂载、预热与热更新，记录实际成功写入、observer 执行、结果、耗时和分配。工具 MUST 校验每个 observer 的输出及回调合同，不得把预设工作量当作实测。

#### Scenario: 稳定依赖非等值更新
- **WHEN** 同一 source 的值实际改变且 K 个 observer 保持依赖
- **THEN** 工具核验所有 observer 都得到新值并各执行一次，输出实测次数和耗时而不使用性能阈值判断正确性

#### Scenario: 动态切换与已脱离 source
- **WHEN** observer 切换依赖后，先更新当前 source 再更新已脱离 source
- **THEN** 当前 source 结果正确且只触发预期回调，已脱离 source 不触发 observer，二者输出分别标识

### Requirement: 离散 effect 测量必须保留有效区间与上传内容

测量工具 SHALL 覆盖 1k effects/256 ranges、10k effects/256 ranges、10k effects/1000 ranges 的真实 geometry/material 更新，保留稳定存量和非相邻区间，经共同上传事务核验最终 ranges、字节、packed 内容与未改邻居。工具 MUST 分开 store 更新与完整同步时间，不把完整同步冒充纯 planning。

#### Scenario: 两组 dirty 的并集与重叠
- **WHEN** geometry/material 发生离散变化，包括交错和重叠索引
- **THEN** 工具观察真实上传回调中的区间与字节，核验改变内容、未改邻居及计数一致，热样本不退化成等值或单个相邻区间

#### Scenario: 上传失败与 epoch 恢复
- **WHEN** 离散更新期间上传或事务提交失败，随后重试或切换设备 epoch
- **THEN** 失败状态不被作为成功结果发布，重试恢复全部目标 packed 内容且旧 epoch 附件无效

### Requirement: 可重复输出必须保留证据边界

测量工具 SHALL 支持可控 iterations/warmup/rounds，输出每次样本和每轮汇总 CSV，包含总量与 p95；MUST 明示不可观测的 subscriber 扫描、排序比较和纯规划成本。实测 baseline 至少三轮并保留所有结果，算法改造须由证据支持。

#### Scenario: 多轮基线
- **WHEN** 在同一正式构建与机器条件下请求三轮测量
- **THEN** 输出各轮独立样本与汇总，并记录参数及构建身份，不挑选最好一轮或把源码推测写成实测改善

#### Scenario: 仅准备源码
- **WHEN** 构建、运行与测量尚未获准执行
- **THEN** 交接声明未编译未运行，所有验收 checkbox 保持未完成，不宣称已确认热点或性能收益
