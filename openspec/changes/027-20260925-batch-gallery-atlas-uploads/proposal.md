# Proposal

## Why

026 已把 Gallery 滚动期 buffer transfer 创建/映射从 2,848 次降至 251 次，但首次挂载累计约 1,077 次 atlas 纹理上传仍各自创建 copy pass 和提交 GPU 命令。该启动路径可能使首次可见帧显著变慢；需要隔离测量后针对提交边界优化。

## What Changes

- 记录 Gallery 首次可见帧的 CPU 阶段时间、atlas 上传数、GPU 上传提交数与实际后端，建立五进程基线。
- 让 Gallery 的 Quad、atlas、Glyph 与 Effect 上传进入同一有序 upload batch；首次挂载与新增 glyph 的纹理上传共用 copy pass 和一次命令提交。
- 批次取消或提交失败时恢复 atlas 页内容的完整上传，并保留 026 的实例全量重传入口。
- Windows MSVC Release 真实 D3D12 五进程复测首次可见帧与固定滚动序列；分别报告 CPU 时间、命令提交与未测 GPU 执行。

本 change 只复用同帧命令/pass，不跨帧复用 transfer、不改变 atlas residency 或目标 texture cycling，也不改变公开 API。

## Capabilities

### New Capabilities

- `gallery-atlas-upload-batching`: 首次挂载 atlas 上传的顺序、失败恢复及可复测提交工作量。

### Modified Capabilities

无。

## Impact

涉及 SDL renderer、Gallery 上传事务、GlyphAtlas 恢复入口与回归测试。实际收益以本机 Windows D3D12 测量为准，不把命令提交减少直接等同于 GPU 执行加速。
