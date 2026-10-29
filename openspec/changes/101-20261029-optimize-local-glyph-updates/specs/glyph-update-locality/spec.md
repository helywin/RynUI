# Spec Delta

## Purpose

确保大型保留场景中的局部字形内容更新保持局部成本，并让连续追加文本的资源增长遵守设备能力与失败恢复合同。性能改进不得改变邻居内容、合法更新的可用性或当前可观察的字形结果。

## ADDED Requirements

### Requirement: Equal length updates remain local

系统 SHALL 在等长字形替换时保持其他范围和内容不变，且不得复制或分配与整个场景大小成正比的临时存储。输入引用当前存储时仍须得到正确结果。

#### Scenario: Four glyph update in a large background
- **WHEN** 在 1k、10k 或 100k 背景字形中替换同样 4 个字形
- **THEN** 仅目标范围内容改变，复制和临时空间不随背景增长，邻居范围及内容保持

#### Scenario: Overlapping input
- **WHEN** 替换源与目标在当前存储中部分或完全重叠
- **THEN** 结果等同先读取完整输入再替换，错误输入或准备失败不留下半更新

### Requirement: Capacity growth respects device limits and recovery

系统 SHALL 在设备资源上限内提供摊销增长，区分实际内容和预留容量。合法内容不得仅因预留容量越界而失败；真正超限须在上传事务前拒绝，上传失败可重试且设备切换使用新的能力。

#### Scenario: Repeated append and restoration
- **WHEN** 连续追加 1,000 个字形并在现有容量内删除、恢复
- **THEN** 分配次数按几何增长而非每次追加增长，新内容完整上传且容量内恢复不重建 buffer

#### Scenario: Device bound and upload retry
- **WHEN** 内容恰好达到设备上限、后续超限，或上传失败后重试及设备 epoch 改变
- **THEN** 合法内容可用，超限在事务前拒绝，失败保留可重试状态且新设备不沿用过时能力
