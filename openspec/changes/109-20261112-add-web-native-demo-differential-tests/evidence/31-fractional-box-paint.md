# 盒控制样本的分数绘制相位

- 固定 Chromium 141 的盒绘制会先将布局位置取整。独立校准页面将分数盒位置表示为整数布局原点加绘制平移，并从实际 computed style 核对二者。文字、正式图标、官方 demo 适配器和留出输入的位置保持原值。Windows 训练板的 Web RGBA 回读逐字节一致。
- Windows 实际 D3D12/DXIL，DPR 1.5。完整留出板的两个方形边框从 1.210 降到 0.036 physical px，局部覆盖误差为 0.0034。三个圆角填充控制的最大轮廓距离为 0.428。真实注入 1 logical px 位移仍测得 1.531 physical px，超过固定上限 0.5。
- Linux GCC Release 在 WSL 私有 Wayland、llvmpipe Vulkan/SPIR-V 的实际 DPR 1 留出板中，两个方形边框的轮廓距离为 0，局部覆盖误差为 0.0035。Windows 和 Linux 均通过锁定浏览器的新增分数相位测试，另通过 41 项定向 Python 测试。已知形状的 AA 支持由其独立几何和原始像素同时验证，局部覆盖逐点参与比较。
- 透明叠层、字形、阴影仍有实测差异。未发布校准 profile，也未使用此局部验证代替完整 pilot 或 meta 结果。
