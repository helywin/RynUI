# Spec Delta

## Purpose

规定同帧 buffer transfer 合并的可观察工作量、数据等价与失败恢复。

## ADDED Requirements

### Requirement: 批量 buffer 上传应有界并保持数据等价
系统 SHALL 在已有 buffer-only batch 中有界地合并 transfer 操作，同时保留每个上传区域的目标 buffer、偏移、字节内容与调用顺序。

#### Scenario: 多目标多区域上传
- **WHEN** 同一帧向 Quad、Glyph 或 Effect buffer 提交多个不连续区域
- **THEN** 提交后每个区域与逐段参考路径的字节内容相同，未写区域保持原值，transfer 创建与映射次数可计数

#### Scenario: 区域超出单块容量
- **WHEN** 一个区域大于普通 staging 块或累计区域跨越块边界
- **THEN** 系统在 SDL 偏移限制内完成该区域且维持原有调用顺序，不截断或错误复用已映射数据

### Requirement: 批次失败应保留完整恢复能力
系统 MUST 在 batch 取消或命令提交失败后，使已被暂时清理的 Quad、Glyph 与 Effect 修改能够在下一次同步完整重传。

#### Scenario: 提交失败后重试
- **WHEN** 编码成功但批次最终提交失败
- **THEN** 下一次同步发送全部有效实例，不能把编码或提交失败当作已经确认的 GPU 状态

### Requirement: 性能结果应有可复测边界
系统 SHALL 在固定 Gallery 滚动窗口报告 CPU 阶段、transfer 创建和映射、区域数与实际后端，并将 GPU 执行时间标为未测，除非另有 GPU profiler 证据。

#### Scenario: Windows 真实窗口复测
- **WHEN** 同一正式 Release 配置运行五个独立 D3D12 Gallery 自动滚动进程
- **THEN** 输出相同 240 帧窗口的工作量和 CPU 对照，并检查退出码与现有终态验收
