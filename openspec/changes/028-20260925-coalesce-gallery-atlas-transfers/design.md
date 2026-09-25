# Design

## Context

027 后首次成功提交帧 CPU 中位数为 377,440 µs，Glyph 资源同步 121,180 µs，纹理区域/transfer 创建均为 1,077，GPU 上传提交仅 1。`SdlSceneRenderer::upload_glyph_texture` 仍逐个创建/map/unmap transfer。SDL 3.4.14 的锁定头文件要求 transfer 在编码前 unmap；本仓库 Glyph 纹理行 pitch 对齐 256 字节、源 offset 对齐 512 字节。

## Goals / Non-Goals

**Goals:** 将同帧相邻纹理区域打包到有容量上限的 transfer；验证 512-byte 对齐、row pitch、chunk 边界、buffer/texture 调用顺序与失败恢复；用相同 Release 首帧场景测 CPU 收益。

**Non-Goals:** 不复用跨帧 transfer，不修改目标 texture cycling，不改 atlas 存储/UV 和 GlyphGpuResources 的逐区域 staging 生成，不声称 GPU 时间下降。

## Decisions

1. **纹理独立 chunk。** 维护一个纹理 transfer mapped 指针、容量/已用字节和有序区域列表。源 offset 逐区域向上对齐 512 字节，默认单块 1 MiB；单个超限区域独占满足其字节数的块。块满时 unmap、在共享 copy pass 中按列表编码、保留 transfer 至命令提交或取消。所有 offset/容量检查遵守 SDL `Uint32` 边界。
2. **与 buffer 块保序。** 收到纹理上传前 flush 正在映射的 buffer 块；收到 buffer 上传前 flush 正在映射的纹理块。由于同一时刻只有一种 active 块，最终 `finish_upload_batch` flush 剩余块并只提交一次。每次 flush 都先 unmap 再编码；不能向已编码 transfer 继续写入。
3. **恢复。** `upload_glyph_texture` 的 batch 路径只在成功 copy 字节后返回；chunk 创建/map/编码/最终提交失败时取消命令并由 Gallery 将现有 atlas 页与 Quad/Glyph/Effect 标为全量重传。旧的非 batch 路径维持原行为。
4. **验收。** 纯布局测试检查多页、不同 row pitch、对齐填充、越界和跨 chunk 顺序；Debug D3D12 真实窗口检查 240 步验收与提交数。Release 完整干净构建后五进程比较首次帧、纹理 transfer 次数及 draw/滚动结果。若首帧 CPU 未超过进程波动，记录不确定而不把操作次数减少当作用户可见加速。

## Risks / Trade-offs

- 一个 1 MiB chunk 可能浪费尾部容量；相较 1,077 个小 transfer 可减少驱动对象固定成本，实际收益由测量决定。
- 多个已编码 chunk 直到单次提交结束才释放；本帧临时 GPU staging 总量随真实上传字节数增长，不跨帧持有。
- `GlyphGpuResources` 仍生成每个区域的 CPU staging vector；本轮仅优化 SDL transfer 对象和映射次数。

## Migration Plan

规划阶段引用 027 实测旧基线；实现、回归、Release 复测与全仓校验分阶段提交，使用英文规范前缀。正式 Windows 构建用 `windows-msvc` / Ninja Multi-Config / MSVC。OpenSpec 1.4.1 不支持数字开头的 `new change` 名称，故先建立字母脚手架再移动到编号路径；无 `doctor` 子命令。
