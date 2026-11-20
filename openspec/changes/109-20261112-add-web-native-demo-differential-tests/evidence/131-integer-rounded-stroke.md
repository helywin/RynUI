# 整数设备网格的一物理像素圆角描边

- Linux GCC Release，私有 Wayland/Vulkan、DPR 1 的原始 Input 回读。无阴影圆角边框的 41 个不同像素仅差一个色阶，分别与独立生成的 Chromium 描边和 R8→BGRA8 描边结果完全相符。描边、填充及源色均来自已提交的实际绘制记录。
- Windows MSVC `windows-msvc-release` 构建校准工具，通过新旧源码形状的定向重放，保持原 Linux profile。和图标复算同时启用后，`initial` 从 1 项圆角轮廓失败降为 0。形状见证回归 11 项通过，偏移、描边宽度及任一原始颜色不满足精确条件时不应用该复算。
- 此次验证覆盖初始检查点。真实 Linux 校准工具在新源码上的平台构建、后续 Input 状态及完整三例仍待复验。
