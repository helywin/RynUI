# Input 圆角背景与描边交界

非 Compact Outlined Input 的背景延伸到不透明圆角描边中心线。半透明描边仍覆盖完整背景。固定 Chromium 的绘制记录显示相同的半描边收缩，比较器从实际观测的均匀不透明边框推导这一允许几何。[Chromium 141 的背景防渗色几何](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/box_painter_base.cc#L950)

MSVC HEADLESS Debug、Release 与 Linux GCC Release 各通过 2/2 项定向 CTest。隐藏 D3D12 1.5× 与私有 Wayland/Vulkan 1× 各完成 3 个 Input 检查点，在固定 policy 硬上限下定向比较均未报告差异。分层 Python 检查通过 42/42，覆盖半透明描边和错误半径/范围的负例。

用旧 Windows profile 的严阈进行诊断，接受、拒绝帧各由 56 条报告降到 23 条，仍有阴影叠色、字形边缘及像素判定差异。该诊断没有重新绑定正式 profile。
