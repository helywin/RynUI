## Purpose

Switch开关应在受控和非受控状态下保持内容与滑块的位置关系，动画需要连续响应状态反向和门禁变化，使不同方向、尺寸和内容类型都能清楚表达当前状态。

## ADDED Requirements

### Requirement: Coherent Switch transitions
Switch SHALL 保持checked/unchecked内容与手柄几何协调，动画反向连续并能停稳。
#### Scenario: Small RTL and rapid reversal
- **WHEN** 小尺寸RTL开关含文字/图标，快速反向或切换disabled/loading
- **THEN** 内容不错误挤压/遮挡，手柄位置与状态一致，身份保留且停稳后不继续请求帧

