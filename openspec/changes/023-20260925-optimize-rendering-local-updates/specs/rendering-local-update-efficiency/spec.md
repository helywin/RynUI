## ADDED Requirements

### Requirement: Dirty domain 稀疏入队
RynUI SHALL 对每个 dirty domain 按 Node slot 与 generation 去重，保留首次入队顺序；清空一个处理轮次后 SHALL 允许同一 live Node 再次入队。

#### Scenario: 重复属性更新
- **WHEN** 同一 Node 在一个轮次中多次触发 Material、Transform 或 HitTest 失效
- **THEN** 每个对应队列只出现该 live Node 一次，队列顺序按第一次入队排列

#### Scenario: slot 重用与下一轮次
- **WHEN** 一个 Node 被销毁且 slot 被新 generation 重用，或 dirty queue 完成一次清空
- **THEN** 新 Node 或下一轮次的更新可以再次入队，旧 generation 不会阻止它

### Requirement: 布局根和显式子树语义
RynUI SHALL 保留现有自动布局失效上溯到树根和显式 `invalidate_subtree` 以给定根为界的行为；不同 dirty domain 不得相互覆盖。

#### Scenario: 局部尺寸与显式子树失效
- **WHEN** 子节点尺寸改变而后显式失效某个容器子树
- **THEN** 自动布局队列使用树根，显式子树队列使用指定根，Material 更新不额外触发 Measure 或 Layout

### Requirement: 一次性 primitive 脏区规划
RynUI SHALL 在 Quad 和 Glyph 实例更新时累积脏区，并在读取上传计划时按实例序号排序及合并重叠或相邻区间。计划 SHALL 包含每个变化实例，并且 SHALL 不包含无关的非相邻实例。

#### Scenario: 乱序且重叠的局部更新
- **WHEN** 一个轮次内对多个实例发出乱序、重复和重叠更新
- **THEN** 读取脏区得到升序、互不相邻且覆盖所有变化实例的最小区间集合

#### Scenario: 结构性 replace
- **WHEN** 变长 replace 搬移后缀实例
- **THEN** 上传计划覆盖搬移后的后缀，旧的移位脏区不越界

### Requirement: Glyph atlas 完整 key 索引
RynUI SHALL 使用完整的 font identity、glyph id、像素尺寸、raster phase 和 mode 区分 atlas entry，并在命中时复用稳定 entry 而不增加脏区。容量或 bitmap 错误 SHALL 不写入索引。

#### Scenario: 命中与区分 raster phase
- **WHEN** 重复插入同一 key，再插入仅 phase 不同的 key
- **THEN** 重复 key 返回原 entry 且不增加上传区间，不同 phase 得到独立 entry

#### Scenario: 容量失败后重试
- **WHEN** 插入因 atlas 容量耗尽而失败，之后重复同一 key
- **THEN** 仍返回容量错误，entry 数量和索引不虚假增加

### Requirement: 可复现的局部工作量证据
RynUI SHALL 提供覆盖不同总规模和脏量的基准或计数输出，并记录所用正式 preset、配置、平台及源码修订。CPU 工作量 SHALL 与真实 GPU 执行时间分开标注。

#### Scenario: 固定脏量扩大场景
- **WHEN** 在固定局部脏量下扩大 mounted Node 或 atlas entry 总数
- **THEN** 基准记录入队、区间规划及查找结果和耗时，能够比较改造前后，而不将 CPU 计时标成 GPU 时长
