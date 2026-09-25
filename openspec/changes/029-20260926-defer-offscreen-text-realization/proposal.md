# Proposal

## Why

028 的 Windows D3D12 Gallery 首帧 CPU 中位数仍为 189,322 µs，首次提交前发生 1,077 个 glyph 纹理区域上传。`TextComponentHost::layout_and_synchronize` 只在本次未布局时跳过视口外文本；首次布局因此会为整个长文档生成绘制资源。布局必须处理尺寸依赖，但绘制资源可以等文本进入可见区域再生成。

## What Changes

- 布局完成后，对视口外且边界确定的文本延后 `TextSceneService::synchronize`，包括首次布局和后续重新布局。
- 文本进入视口时按当前内容、字体、位置和滚动 phase 生成 glyph，避免旧绘制资源成为可见结果。
- 增加平台通用回归，覆盖首次布局、离屏内容改变和重新进入视口；以相同五进程真实 D3D12 Gallery 首帧及 240 步滚动场景复测。

本 change 不跳过文本 measure，不改变公开 API、字体 raster 算法、atlas 容量或文本视觉样式。

## Capabilities

### New Capabilities

- `offscreen-text-realization`: 在保持布局与重新可见语义的前提下延后视口外文本绘制资源生成。

### Modified Capabilities

无。

## Impact

涉及文本宿主可见性判断、回归测试与 Gallery 真实窗口首帧 telemetry。旧基线使用 028 的 `gallery-texture-chunk-after.csv`；CPU 与上传工作量分别报告，GPU 执行时间仍未由本 change 测得。
