# Proposal

## Why

审查 TUNE-PERF-001 确认局部等长 Text 更新仍复制整个 glyph store。TUNE-PERF-002 的几何扩容已由 100 实现，但持续追加、能力上限及失败恢复还需要针对性验收。以 `dd3e71f775fb8d4f1f46aeeff5ea444ba816e54d` 为代码基线，减少局部更新的全局成本。

## What Changes

- 为等长 glyph 替换增加 alias-safe 原位路径，保持 range、邻居、dirty 与失败前状态。
- 复用 100 的 GPU buffer 容量策略，补持续追加、边界、删除恢复与失败重试；仅修复实际暴露的兼容问题。
- 添加可重复的局部更新计数与微基准，区分逻辑 glyph 数、分配容量及上传字节。
- 非目标：变长 store 架构重写、字体/Theme/GPU ABI、其他组件和 Gallery 布局。

## Capabilities

### New Capabilities

- `glyph-update-locality`：局部字形更新与存储容量增长的成本及兼容合同。

### Modified Capabilities

无。当前主 specs 目录为空；本次不改变已有公开 Text/Input 行为。

## Impact

主要涉及 `src/graphics/glyph_scene.cpp`、`src/renderer/common/glyph_gpu_resources.*` 及相邻测试/基准。风险为自引用 span、dirty 分配失败、能力上限和新后缀上传遗漏。验收使用正式 MSVC HEADLESS Debug/Release；不把逻辑测试等同 GPU 或高刷体验通过。
