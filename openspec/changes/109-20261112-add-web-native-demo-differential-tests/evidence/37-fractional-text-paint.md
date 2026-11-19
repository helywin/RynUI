# 文字校准块的小数绘制位置

- 固定 Chromium 在 Windows 与 Linux 的定向测试都验证了边框和文字的真实绘制矩阵、逻辑位置及 pinned 字体。校准块的小数位移由浏览器执行，截图和比较器未重采样。
- Windows D3D12/DXIL 与 Chromium 的 DPR 1.5 留出板局部复算中，文字最大轮廓差由 2.890 降至 2.440 physical px，内部颜色最大 ΔE00 由 77.91 降至 4.71。Linux 私有 Wayland/Vulkan 的 DPR 1 留出板文字指标逐项相等。两平台仍有独立字形轮廓失败，未发布配置或宣称 paired 通过。
