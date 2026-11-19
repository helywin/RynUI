# Select 弹层阴影的物理像素覆盖

- Select 弹层与有限校准控制使用物理像素上的三次盒式滤波。外阴影按像素内圆角面积镂空，保留祖先硬裁剪和透明度语义，原有效果参数继续支持既有连续阴影。
- Linux `linux-gcc-release` 的 private Wayland/Vulkan 原生回读与固定 Chromium 在 DPR 1 比较。训练和留出板各包含不透明、半透明、透明主体三种阴影，六组质量相对误差均不超过 0.0123，投影相对误差不超过 0.0249，最大逐像素差不超过 0.0157。以上小于原有 0.05、0.05、0.02 上限。定向阴影和 shader 合同 CTest 8/8。
- Windows `windows-msvc-headless` 的阴影与后端能力定向 CTest 在 Debug 和 Release 各通过 2/2。 `windows-msvc-release` 的 shader 合同 CTest 7/7。hidden D3D12 原生回读与固定 Chromium 在 DPR 1.5 比较，六组质量误差为 0.0314–0.0707，投影误差为 0.2019–0.3979，最大逐像素差为 0.0235–0.0393。分数缩放的阴影比较仍不通过。
