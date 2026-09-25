# Proposal

## Why

027 将 Gallery 首次帧 atlas 上传从 1,080 次命令提交合为一次，使首帧 CPU 中位数从 473 ms 降至 377 ms；但 1,077 个 atlas 区域仍各自创建和映射 transfer，首帧 Glyph 资源同步仍约 121 ms。继续减少纹理 transfer 的固定成本有明确工作量基线。

## What Changes

- 将连续 atlas 纹理区域按 SDL 要求的 512-byte 源 offset 对齐装入容量受限的同帧 transfer 块，每块只创建、map/unmap 一次。
- 保持每个纹理区域的 row pitch、目标页与 rectangle、调用顺序，且在编码前 unmap；buffer 与纹理块交替时保持原先依赖顺序。
- 保留 027 的 atlas 全页失败恢复及 026 的实例全量重传，在 Windows MSVC Debug 定向测试和真实 D3D12 窗口验证。
- 使用 027 相同五进程首次帧 telemetry 与正式 Release 构建复测，报告 transfer 次数、首帧 CPU 和滚动终态。

本 change 不改变字体 raster、atlas packing/residency、目标纹理 cycling 或公开 API，也不跨帧复用 transfer。

## Capabilities

### New Capabilities

- `gallery-atlas-transfer-coalescing`: 同帧纹理 transfer 分块、对齐和结果等价。

### Modified Capabilities

无。

## Impact

涉及 SDL renderer 的上传批次及纯布局测试。基线复用 027 的五进程 D3D12 `gallery-atlas-after.csv`，不重复运行相同旧版本。GPU 执行时间仍须独立工具测量。
