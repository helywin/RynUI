# Design

## Context

`GlyphGpuResources::upload_atlas` 对每个 dirty rectangle 调用一次 `SdlSceneRenderer::upload_glyph_texture`，后者各自创建、map/unmap transfer、获取 command buffer、开启 copy pass、提交并释放。026 的 upload batch 仅覆盖 buffer，Gallery 在 atlas dirty 或首次创建 Quad buffer 时不启用该批次。首次挂载的纹理提交数量需通过独立 telemetry 确认。

## Goals / Non-Goals

**Goals:** 将首次可见帧拆分并测量；以单一有序 copy pass 提交同帧纹理和 buffer 上传；失败后重传所有有效 atlas 页及实例；维持原始纹理 row pitch、offset 对齐和 draw 顺序。

**Non-Goals:** 不合并纹理 transfer 映射，不做跨帧资源复用、GPU residency 淘汰或 target cycling；不声称 GPU 执行时间被 CPU 阶段计时证明。

## Decisions

1. **独立首次帧基线。** Gallery 在首次 `submit_frame` 记录整帧、资源/纹理同步及上传提交增量，不将其与后续 240 个滚动帧平均；五次独立 Release D3D12 运行保持窗口、字体和 preset 一致。
2. **统一同帧提交。** 现有 buffer batch 增加纹理上传编码。若 buffer transfer 正处于 mapped 状态，先 unmap 并编码该 chunk，再编码纹理；随后可继续映射新的 buffer chunk。每个纹理保留现有 transfer 和 row pitch，直到提交或取消后才释放。copy pass 保持源调用顺序，纹理数据在依赖它的 glyph draw 前完成提交。
3. **恢复边界。** Gallery 在首帧就开始 batch。已编码但未成功提交的纹理视为未确认；取消或 finish 失败时，把已存在的 atlas 各页重新标记为完整 dirty rectangle，下帧重传，并对 Quad/Glyph/Effect 沿用 026 的全量恢复。首次未创建资源的路径也可取消。
4. **验收。** 平台通用测试验证 atlas 全页重传计划、纹理区域/字节与提交失败恢复入口。Windows MSVC Debug 定向测试与 D3D12 Gallery 验收后，用 Release 干净构建跑五次首次帧和滚动；如首次帧未明显改善，记录结果而不把提交数下降冒充响应延迟下降。

## Risks / Trade-offs

- 一个 command buffer 含较多 copy 命令，CPU 编码与 GPU 队列工作仍存在；只减少 pass/submit 固定开销。
- Atlas 单页重传用于失败恢复，可能超过原增量区域字节数；只在失败后执行，不进入稳态。
- 纹理 transfer 在命令提交前必须保持有效；取消、提交失败及析构均释放所有 transfer，不保留跨帧引用。

## Migration Plan

按规划、首次帧 telemetry 与基线、实现和回归、真实窗口复测、全仓校验分别提交。正式 Windows 构建使用 `windows-msvc` / Ninja Multi-Config / MSVC。OpenSpec 1.4.1 不接受数字开头的 `new change` 名称，脚手架以字母名称创建后移入仓库要求的编号路径；该版本无 `doctor` 子命令。
