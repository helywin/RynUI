# Outlined Input 边缘回归

普通 Outlined Input 先绘制完整圆角背景，再绘制独立的空心描边。原先描边后的内缩背景会在内缘重复混合覆盖率，导致警告色边缘偏浅。Compact 分段沿用现有几何。

Windows MSVC Release 的原生 Input retained paint 回归通过，断言背景覆盖整个 border box、描边为空心且在背景之后提交。D3D12 隐藏窗口在实际 1.5× DPR 捕获 Input 三个检查点。使用固定 Chromium 参考，在每个检查点四个 10×10 物理像素角区比较，逐通道最大差初始态从 62 降到 2，接受和拒绝态均从 57 降到 7。三个检查点角区中差值至少 8 的通道样本合计从 286 降到 0。这次只验收角区修复，完整差分判定仍按 pilot 执行。
