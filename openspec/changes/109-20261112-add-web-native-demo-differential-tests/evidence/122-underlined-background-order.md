# 下划线的绘制顺序

Underlined Input 先发布背景，再发布底部边框。Select 复用这一绘制路径，底部边框从完整覆盖的背景上露出。实际片段命令顺序回归覆盖 Filled 和 Underlined。

Windows MSVC HEADLESS Release 的 `input_paint_snapshot`、`portable.input_variant` 和 `portable.select_component` 共 3/3 通过。Windows D3D12 回读的十个 Select 检查点均恢复底线。逐点核对底线内部 319 像素，九个检查点与 Chromium 一致；`async-delivered` 的 Web 相机帧仍呈现悬停色，该状态差异保持开放。
