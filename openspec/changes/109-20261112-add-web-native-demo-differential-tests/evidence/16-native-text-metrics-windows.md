# Windows 逻辑文字度量回归

可缩放字体的逻辑 advance 和行度量与 FreeType 栅格密度分离。固定行高内由主字体确定基线，后备字形继续贡献实际墨迹边界。

- Windows MSVC HEADLESS Debug、Release 定向 CTest 各通过 10 项，覆盖字体、文本布局、实际文字观测、Input、Typography、Select 和 Compact。
- Windows MSVC Release、隐藏 D3D12/DXIL、DPR 1.5 对同一份固定训练板回读。与既有 Chromium 141.0.7390.37 回读相比，192 个文字控制中，超出 1 个物理像素轮廓距离的控制从 100 个降至 85 个，最大距离从 4.00 降至 3.16。600 字重 20 px 的 `M` 逻辑 advance 从 19.0 变为 18.578125，Web 实测为 18.59375。

视觉证据来自单次训练板诊断。剩余轮廓差异、重复校准、留出集、其他 DPI 和 Linux 继续保留开放状态，无 paired 或 profile 通过结论。
