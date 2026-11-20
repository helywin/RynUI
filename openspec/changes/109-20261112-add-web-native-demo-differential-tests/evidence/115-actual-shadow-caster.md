# 阴影使用实际裁剪轮廓

原生观察携带实际 exterior shadow 的 caster 内缩。Web 侧按锁定 Chromium 已观察的背景绘制计算局部裁剪。阴影自身的偏移和扩展保持独立。精确落在物理网格上的方形边框和背景保留固定覆盖率，已被上层完全遮住的阴影不参与该像素的逆向求解。校准阈值保持原值。[Chromium 141 BoxPainterBase](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/box_painter_base.cc#L158)

45 项分层 Python 测试以及 MSVC HEADLESS Release 原生输入与观察测试 2/2 通过。Windows D3D12 采集 Button 四个和 Input 三个检查点。用已有严格配置对同批截图重放，Button 的差异记录从 251 降到 204，Input 的结果完全一致。Select 虚拟弹层检查点也使用实际裁剪完成了整帧与分片比较，仍有像素和字体差异。
