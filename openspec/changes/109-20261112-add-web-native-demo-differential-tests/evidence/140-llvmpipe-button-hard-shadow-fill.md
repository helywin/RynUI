# Linux Button 硬阴影与圆角填充的 RGB8 合成

Ubuntu 24.04，`linux-gcc-release`，私有 Wayland/Vulkan，Mesa llvmpipe 25.2.8，DPR 1，锁定 Chromium 141.0.7390.37。Button 正式 pilot 初始检查点的圆角处，两端使用相同的硬阴影和不透明填充几何。对有实测 llvmpipe BGRA8 能力的整数物理几何，按分别编码源色和目标色贡献后再相加的路径计算原生填充。

- 使用新的 C++ 控制程序重放 `058b4808` 的两个圆角区域，共 286 个原始像素。24 个 Web/native 不同的像素均和各自独立编码精确匹配；把原生值改一个色阶后 24 个都拒绝。原合并取整模型预测错了 12 个原生像素。未改变准入阈值或原图。
- `rynui_demo_differential_calibration` 增量构建通过。定向 Python 64/64 通过，包含非整数位置、其他 DPR 和未观察到 llvmpipe 路径的负例。

原始数据保存在 `out/formal-pilot-linux-058b4808/` 与 `out/nov19-differential/`。完整 Button、三例联合运行和故障注入继续待验收。
