# 圆角背景的部分覆盖

把同一物理位置、半径和材质的均匀不透明圆角填充纳入固定轮廓采样。透明填充、不同角半径和未对齐的物理坐标沿用原判定。使用截图重放时，底色和组透明度仍参与实际预测。[Chromium 141 的圆角背景路径](https://raw.githubusercontent.com/chromium/chromium/refs/tags/141.0.7390.37/third_party/blink/renderer/core/paint/box_painter_base.cc#L907)

Windows MSVC 原生 Release 控制程序返回预期的 R8 覆盖字节 248，带描边宽度的填充请求返回 4 且未生成回执。63 项 Python 形状、分层和区域测试通过。使用相同的 D3D12 与 Chromium 截图和原阈值重放，Button 四个检查点的差异合计从 204 降至 189，Input 仍为 36。整例验收保持开放。
