# 直角填充和描边的物理覆盖

- Windows MSVC HEADLESS Debug 和 Release 各通过 8 项定向 CTest，覆盖 Quad、效果数学和打包参考、合成、Button、Selection、Space Compact 与真实校准控制几何。Windows MSVC Release 通过 5 项 shader 合同和隐藏 D3D12 合成回读测试。另通过 38 项定向 Python 测试、shader 闭包负控制和格式检查。
- Windows D3D12/DXIL 与固定 Chromium 的实际训练板回读（DPR 1.5）。黑底白框与白底黑框的物理轮廓距离均从 0.197 降至 0，固定局部覆盖误差从 0.1255 降至 0.0059。删除右边框的实际故障仍只提交三条边。
- Linux GCC Release 在 WSL Ubuntu 24.04 私有 Wayland、llvmpipe Vulkan/SPIR-V 通过 5 项 shader 合同和合成回读测试。实际 DPR 1 训练板的两个方形边框与固定 Chromium 对应区域逐像素相等。
- 留出板在相同半逻辑像素位置仍失败，新方形边框的轮廓距离为 1.210 physical px。实际 Web 直边比声明位置偏移 0.746 physical px，与 Blink 在绘制盒背景和边框前取整一致。量测保留原图和原始坐标。上限仍为 0.5，未发布校准 profile。[Blink 边框绘制源码](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/contoured_border_geometry.cc)
