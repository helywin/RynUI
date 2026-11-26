# Linux Vulkan 加载态阴影的 8 位舍入

- 在已构建的 `linux-gcc-release` renderer 上使用独立私有 Wayland/Vulkan llvmpipe 回读探针。对三个相邻透明度和 2,058 组通道输入检查透明与不透明目标。先分别将源通道和 alpha 舍入至最近偶数，再与白底合成，实测与该模型全部一致。
- `windows-msvc-release` 校准工具重新构建后，复验锁定 Chromium 和 Linux Vulkan 的 Button 初始阴影回读。限定视窗中阴影投影相对误差从 0.075472 降至 0。替换一个原生 RGB 像素后，联合颜色贡献差异为 0.026411，超过既有阈值 0.006862。只有两侧实际像素分别精确匹配时才采用该见证。
- 通过 69 项定向 Python 测试。重放 Windows 隐藏 D3D12 与锁定 Chromium 回读，原有四个加载态阴影检查点均保留零投影误差，单像素故障仍超阈值。正式 pilot 与 meta 继续独立验收。
