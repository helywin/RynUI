# 不透明填充下的外阴影边缘

- 背景完全不透明时，未偏移的阴影镂空边界内收一个逻辑像素，允许后绘制的背景遮挡主体内部。半透明背景、淡入淡出和祖先透明度仍使用完整主体镂空。依据为 [Chromium 固定版的主体阴影裁剪](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/box_painter_base.cc)。
- `linux-gcc-release` 私有 Wayland/Vulkan 读回训练板和留出板。留出板的不透明阴影覆盖量相对误差从 6.55% 降至 1.23%，投影从 22.86% 降至 5.20%，轮廓采样最大误差从 0.02745 降至 0.01176。留出板投影仍高于 5% 上限。
- `windows-msvc-release` 隐藏 D3D12/DXIL 读回同类控制。留出板最大采样误差从 0.04314 降至 0.03137，覆盖量误差从 12.51% 升至 16.10%。两平台的半透明和透明控制读回指标保持一致。阴影模糊核与设备像素相位继续验收。
- Windows MSVC HEADLESS Debug、Release 的 `rynui.portable.rounded_effect_gpu` 各通过 1 项。Linux Release shader contract、layout、closure 7/7 通过，Python 校准定向检查 21/21 通过。
