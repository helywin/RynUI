# 有额外高度的文本行基线

- Windows MSVC HEADLESS Debug 和 Release 各通过 14 项定向 CTest，覆盖 TextEngine、多行和紧凑行、Input 光标、图标、Button、Select、Space 及观测合同。校准 Python 定向测试 11/11，格式检查 1242 个文件通过。
- 固定 Chromium 的 HTML 行框使用整数主字体行度量，将额外高度向下取整后分配给行框上半部分。原生在有额外高度时采用这一位置规则，继续使用真实的行高、字形 advance 和 fallback 墨迹。 [Chromium 141 的 FontMetrics](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/platform/fonts/font_metrics.h) · [行间余量规则](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/layout/inline/line_utils.cc)
- Windows 隐藏 D3D12/DXIL、DPR 1.5 以及 WSL Ubuntu 24.04 私有 Mutter Wayland 上 llvmpipe Vulkan/SPIR-V、DPR 1 各重拍 192 个单字控制。两平台的最大排版基线差均由 0.760 降为 0 logical px。Windows 文字综合 geometry 的剩余值约 0.016 logical px，最大墨迹轮廓距离仍为 3 physical px。
- 阴影、圆角和文字墨迹仍有超限值，未发布校准 profile。
