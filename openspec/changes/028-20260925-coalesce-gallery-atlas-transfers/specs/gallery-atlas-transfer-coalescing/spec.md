# Spec Delta

## Purpose

规定同帧 atlas 纹理 transfer 合并的对齐、顺序与失败恢复边界。

## ADDED Requirements

### Requirement: 纹理上传分块应保持结果等价
系统 SHALL 在容量受限的 transfer 块中放置多个 atlas 区域，并保持每个区域的目标纹理、rectangle、row pitch、字节和原始调用顺序。

#### Scenario: 多区域跨 chunk
- **WHEN** 同一帧纹理上传总量超过一个 chunk 或单个区域大于默认容量
- **THEN** 所有区域在 SDL 32-bit 范围内完整编码，源 offset 满足 512-byte 对齐，chunk 在编码前 unmap，目标内容与逐区域参考路径相同

#### Scenario: buffer 与纹理交替
- **WHEN** 同一 batch 依次提交 buffer、纹理、buffer 区域
- **THEN** 编码顺序与调用顺序一致，绘制前只在批次完成后确认资源状态

### Requirement: 失败后应可完整重传
系统 MUST 在 chunk 失败、batch 取消或提交失败时，保留现有 atlas 页及实例 buffer 的完整重传能力。

#### Scenario: 纹理块已编码但提交失败
- **WHEN** 一个或多个纹理块已编码而最终 GPU 命令提交失败
- **THEN** 下次同步重传所有有效 atlas 页与实例，不沿用未确认的 dirty 清理状态

### Requirement: 性能结论应以真实窗口为依据
系统 SHALL 对照相同五进程 D3D12 首次帧与滚动场景报告 transfer 次数和 CPU 时间，并标注 GPU 执行时间未测。

#### Scenario: 首次帧复测
- **WHEN** Windows MSVC Release Gallery 首次帧含大量 atlas dirty rectangle
- **THEN** 报告原始数据、首帧阶段中位数、纹理 transfer 数、上传提交数、draw 数及验收退出码
