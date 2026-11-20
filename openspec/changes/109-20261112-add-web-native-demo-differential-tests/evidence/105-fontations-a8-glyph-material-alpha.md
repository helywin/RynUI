# Fontations 黑色字形的材质 alpha

固定 Fontations 灰度传递同样在字节遮罩与材质 alpha 之间采用整数乘积。DirectWrite 的对应路径和普通覆盖率传递各自保持既有结果。独立字体见证按实际传递方式换算材质覆盖率。

Linux GCC Release / 私有 Wayland Vulkan 与 Windows MSVC Release / 隐藏 D3D12 的 shader、合成与真实回读分别通过 3/3。原生回读用同一遮罩分别验证两种灰度传递。分层比较测试 40/40 通过。

Linux DPR 1 实际 Input 三个检查点重新采集。用原 Linux 严格校准阈值重放，首帧从触及 64 条报告上限降为 4 条边缘差异。接受与拒绝检查点仍有焦点边缘和重叠颜色贡献差异。原始图像留在 `out/`。
