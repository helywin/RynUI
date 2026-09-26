# Proposal

## Why

029 将 Gallery 首帧纹理上传从 1,077 区域降至 257 区域，但滚动到未实现化文本时出现最长约 15 ms 的 CPU 帧。对第 240 步长距离跳转的临时诊断显示该帧约 15.4 ms，其中约 12 ms 位于文本宿主的同步循环，新增字体 raster 为 55 次。`TextSceneService::synchronize` 每生成一个文本 primitive 就重建全部文本 ordered scene；同一帧多个文本进入视口时，这项全量工作重复执行。

## What Changes

- 给文本场景增加仅在宿主同步轮次内使用的有界批次，将多次 primitive 改变合为一次 ordered scene 重建。
- 普通单记录 `synchronize`、destroy 等入口保持调用结束后 ordered scene 可读；失败和异常不得让下一次成功帧读到陈旧顺序。
- 增加批次与逐条参考路径等价性测试，复测 029 的五进程 D3D12 首帧与固定 240 步滚动，特别报告最长帧。

本 change 不改变 glyph raster、atlas 策略、绘制顺序语义或公开组件 API。

## Capabilities

### New Capabilities

- `text-scene-order-batching`: 同一宿主同步轮次内合并 ordered scene 重建并保证结果等价。

### Modified Capabilities

无。

## Impact

涉及内部 `TextSceneService`、`TextComponentHost`、平台通用测试及 Gallery 真实窗口验收。旧性能基线为 029 的 `gallery-offscreen-after.csv`。CPU 阶段计时不代表 GPU 执行时间。
