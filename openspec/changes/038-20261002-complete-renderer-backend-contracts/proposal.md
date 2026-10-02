# Proposal

## Why

037 已隔离逻辑 scene 与 GPU packing，但上传接口仍混合源数据与 GPU transfer 布局，后端也没有统一声明必需能力与输入限制。按用户要求，本 change 集中收口现有 renderer 的数据、能力与恢复合同，使后续组件继续沿 logical scene 开发，无需逐个修补 backend 分支。

## What Changes

- **BREAKING（内部 renderer API）**：GlyphTextureUpload 改为 R8 源字节视图、source offset、source row stride 与目标 rectangle；移除 transfer offset、pixels-per-row、rows-per-layer 和共同接口上的 backend row-alignment capability。
- common 直接借用 atlas 原始 page/dirty plan，不创建 backend 对齐 staging；backend 在 upload 成功返回前拥有所需字节，借用只持续至调用返回。
- SDL 在自己的 mapped transfer buffer 中逐行复制并零填充 padding，保留 256-byte row pitch、512-byte batch offset、批量/chunk 上传与独立上传路径；Recording 仅保存目标区域的紧凑像素。
- 建立统一 R8 源/目标范围及整数溢出校验；保留 begin/commit/cancel、失败 dirty 重试、atlas bytes、绘制顺序和 shader ABI。
- backend 显式声明 logical scene/packed ABI 版本、三类 primitive、R8 atlas、顺序/局部上传能力及输入 buffer/texture 限制；缺少必需能力在创建场景资源前拒绝，超限场景在 begin/upload 前拒绝。限制是 backend 接受的输入上限，不冒充未知硬件预算。
- SDL 的 R8 sampling 能力从锁定 SDL 的实际 device 查询；Recording 提供可配置的限制/能力 fixture，验证 unsupported、超限、恢复和跨 owner/epoch 行为。
- 补充真实 atlas 源视图、偏移/stride/短末行、后端复制所有权、取消/失败/重建和 SDL transfer layout 合同，并执行原生 Windows 验收。

## Capabilities

### New Capabilities

- `renderer-texture-uploads`：源纹理数据与后端传输布局分离、范围校验、字节所有权和事务恢复。
- `renderer-capabilities`：必需视觉能力、scene ABI 兼容性与 backend 输入限制的显式协商和提前拒绝。

### Modified Capabilities

无；当前主 specs 目录为空，035–037 的 scene/事务边界保持有效。

## Impact

涉及 renderer/common Glyph resources、新的 R8 source 合同、SDL texture upload、Recording、相关 test doubles、架构与 renderer 文档。不修改公开组件 API、atlas 格式、shader、依赖或平台构建组合；不实现 WebGPU/GLES/移动宿主、异步 GPU 完成、自动 device-loss 恢复或新 OS 支持。

风险主要是 source offset/stride 边界、mapped buffer padding、batch chunk 大小、能力误报及 dirty 提交顺序；以 literal 像素、正负能力 fixtures 与真实窗口验证，CPU 合同不能代替 GPU/平台证据。本 change 在 Windows 执行平台通用验证和 Windows 原生验收，Linux 原生项单独保留。框架收口后进入独立组件 change，避免把具体组件语义塞进 backend 接口。
