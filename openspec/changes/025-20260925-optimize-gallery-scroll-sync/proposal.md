# Proposal

## Why

Windows MSVC Release 的真实 D3D12 Gallery 滚动五进程测量显示，923 个 Node、61 条交互的窗口平均整帧 CPU 中位数为 16,798 µs，`layout_and_synchronize` 外层为 14,046 µs，命中刷新仅约 5 µs。当前阶段计时不能区分文本宿主、参与者、effect、fragment 与 scene composer 的耗时；继续优化需要先确定工作量和真实热点。

## What Changes

- 为 Gallery 滚动同步增加可复测的子阶段 CPU 计时及访问/更新计数，建立改造前五进程基线。
- 依据基线选择最昂贵的重复工作，缩小对滚动中未变化或离屏内容的同步范围，并保持文字清晰度、裁剪、scene 顺序和命中坐标。
- 使用平台通用回归与 Windows D3D12 真实窗口对照验证；分别报告局部工作量和整帧收益，GPU 执行仍标为未测。

本 change 限于现有 Gallery 滚动同步路径；不引入共享 GPU transform、primitive arena、虚拟化或公开 API。

## Capabilities

### New Capabilities

- `gallery-scroll-synchronization-locality`: 滚动同步的观测、结果等价与无关工作边界。

### Modified Capabilities

无。

## Impact

可能涉及 `TextComponentHost`、`TextSceneService`、参与者同步及 Gallery telemetry。诊断结果将写入 design 后再选定代码改造点。Windows 真实窗口与平台通用测试分开验收；不外推至 Linux 或 GPU 执行时间。
