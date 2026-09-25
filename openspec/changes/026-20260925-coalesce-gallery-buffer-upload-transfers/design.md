# Design

## Context

Gallery 滚动时 Glyph 实例存在多个脏区。`SdlSceneRenderer::begin_buffer_upload_batch` 已将 buffer 上传放进一个 copy pass，但 `upload_buffer` 仍逐段创建和映射 transfer。`GlyphGpuResources`、`QuadGpuBuffer` 与 `RoundedEffectGpuResources` 会在上传调用返回后清理 dirty；批次真正成功提交发生在 `finish_buffer_upload_batch`，失败时需恢复完整数据。锁定的 SDL 3.4.14 要求 transfer 在编码上传命令之前 unmap；目标 buffer 的 partial update 继续使用 `cycle=false`。

## Goals / Non-Goals

**Goals:** 量化 transfer 创建/映射固定成本；在保持区域顺序和目标 buffer 内容的条件下，限制每帧 transfer 数量；批次失败后完整重传；通过真实 D3D12 场景衡量 CPU 效果。

**Non-Goals:** 不声称 GPU 执行时间下降；不在跨帧复用 transfer；不以 target cycling 替代部分更新；不改变 atlas texture 上传路径。

## Decisions

1. **先测工作量。** 在 renderer 计数中区分 buffer 与 texture transfer 创建、映射以及 buffer 上传区域。Gallery 固定捕获自动滚动最初 240 个提交帧，同一可执行文件运行五个独立进程。
2. **批内有界打包。** `upload_buffer` 在 batch 中复制字节到 CPU 侧待上传区域，保留目标句柄、偏移和原始顺序。`finish` 按容量上限分块创建 transfer，每块只 map/unmap 一次，按 SDL 合同 unmap 后编码区域；单个超限区域独占一块。不得越过 32-bit SDL 偏移或静默丢弃字节。比较基线后确定容量及必要的对齐。
3. **失败恢复。** 编码和提交成功前 CPU store 的 dirty 清理只是暂时状态；Gallery 在 batch 取消或 finish 失败时，将 Quad/Glyph 全部有效实例标为 dirty，并使 effect 下次走完整上传。成功后不增加重复上传。atlas 不参与该 buffer-only batch。
4. **测试与复测。** 平台通用测试对照逐段参考上传，覆盖顺序、目标/源偏移、跨块、边界和取消/提交失败后的重传。Windows MSVC Release 干净构建后，用原有 `--scroll-acceptance` 五进程比较每帧 Glyph/资源同步与整帧 CPU；如果收益被额外 CPU 拷贝抵消，只报告结果并重新选择实现，不以 transfer 次数下降冒充整体加速。

## Risks / Trade-offs

- CPU staging 会多一次拷贝和持有至批次结束的内存；设置明确上限与容量统计，不保留跨帧大 buffer。
- 目标 buffer 可能在同步期间扩容；区域必须引用仍有效的目标，取消和失败时完整重传。
- SDL transfer 生命周期、失败与异步 GPU 使用由 SDL 提交/释放合同管理；本 change 不假设提交意味着 GPU 完成。

## Migration Plan

依次提交规划、计数及基线、平台通用回归、实现、真实窗口复测与集成结果。各阶段验收通过后用英文规范前缀提交。正式 Windows 构建使用 `windows-msvc` preset / Ninja Multi-Config / MSVC；Linux GPU 结果不作外推。OpenSpec 1.4.1 的 CLI 不接受数字开头 change 名称且没有 `doctor` 子命令，脚手架先以字母名称创建，再按仓库规则移动到编号路径；strict validate 可对编号路径执行。
