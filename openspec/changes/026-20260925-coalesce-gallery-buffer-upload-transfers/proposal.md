# Proposal

## Why

025 的 Windows D3D12 Gallery 滚动复测将整帧 CPU 降至约 4.13 ms，但资源同步仍约 1.32 ms，其中 Glyph 实例同步约 0.97 ms。现有 buffer-only batch 共用一次 copy pass 与命令提交，却为每个脏区分别创建、映射、释放 transfer buffer。需要先量化这些操作，再减少同帧资源准备开销。

## What Changes

- 为固定 240 帧 Gallery 滚动增加 buffer/texture transfer 的创建、映射、上传区域计数，记录五进程基线。
- 在已有 buffer-only batch 内把多个 buffer 上传区域装入有界的同帧 staging transfer；保持各目标偏移、写入顺序与 `cycle=false` 的局部更新语义。
- 批次取消或提交失败时使受影响的 CPU store 可完整重传；增加区域边界、数据快照及失败恢复回归。
- 用正式 Windows MSVC Release 和真实 D3D12 Gallery 复测，分别报告 transfer 工作量、CPU 时间和未测的 GPU 执行时间。

本 change 不复用跨帧 transfer、不修改目标 GPU buffer cycling，也不引入新的公开 API。

## Capabilities

### New Capabilities

- `batched-buffer-transfer-staging`: 同帧 buffer 上传的合并、确认边界和可复测工作量。

### Modified Capabilities

无。

## Impact

涉及 SDL renderer 的上传批次、Gallery 诊断、必要的 store 恢复入口与平台通用测试。性能结论仅覆盖本机实测后端；失败恢复以注入测试验证，不能把命令提交视为 GPU 已执行完成。
