# 正式图标的实际位置与墨迹

- Windows MSVC Release 和 Linux GCC Release 校准程序均编译通过。Windows 校准 Python 定向测试 12/12，覆盖 SVG 视口偏移、原生字形 atlas 原点偏移和无效输入。
- Windows 隐藏 D3D12/DXIL、DPR 1.5 与 Linux 私有 Mutter Wayland 上 llvmpipe Vulkan/SPIR-V、DPR 1 各重拍 40 个图标控制。两平台各自的原生 RGBA SHA 与增加观测前一致。实际位置、独立轮廓及一个物理像素的支持域检查通过。
- 配对差异仍然存在，Windows 与 Linux 的最大图标轮廓距离分别为 1 和约 1.41 physical px。图标局部覆盖及颜色也仍超限，未发布校准 profile。
