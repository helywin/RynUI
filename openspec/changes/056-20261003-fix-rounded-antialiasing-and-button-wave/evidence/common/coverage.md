# 1.1 共同圆角 coverage 验证

2026-10-03，实际 Windows/MSVC，Ninja Multi-Config，BUNDLED。`windows-msvc-headless` Debug/Release 的 rounded_effect math/store/scene/gpu/resources/allocation 与 backend/Core boundaries 各7/7，1.85s / 1.72s。`windows-msvc` Debug Gallery实际重建DXIL/SPIR-V、reflection/deploy，generated_shaders/deployed_shaders/rounded_effect_shader_contract 3/3，0.10s。

零 blur 从硬阈值改为一个物理像素的 smooth coverage；inset mask连续、bounds含AA guard，blur>0 Gaussian和硬ancestor clip保留。GPU ABI仍112 bytes；logical AA width=1/scale、packed width=1。曲面边界验证内/外/中点分数coverage，1/1.25/1.5/2分数坐标网格逐点比较 logical/packed reference，独立四象限fill检查split-line与曲面一致，中心不透明、远处透明。Quad已存在导数AA、outline已使用物理宽度，此阶段未改其实现。

clang-format 22.1.3检查458 owned sources；doctor healthy、strict56/56、diff check通过后提交。本阶段证明共同数学、实际生产headless边界与shader产物合同；真实GPU像素/用户TextArea与动画图像留给Windows独立3.1。
