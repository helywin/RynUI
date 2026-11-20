# 有限字形与圆角比较的量化

29 项分层比较测试通过。负控制覆盖错色、错绘制归属、设备位置移动、双侧缺墨以及 0.5 logical px 以外的 advance 偏差。指标的独立校准步骤与 hard cap 保持原定义。

Linux 私有 Wayland/Vulkan 与固定 Chromium 实际 1× 的 Input 三个检查点，经定向比较均在固定 policy 上限内为零差异。Windows 1.5× 的旧已校准 profile 作诊断时，初始检查点的报告项从 854 降到 314。严格 profile 仍有轮廓、颜色和阴影叠色差异，本次未给出三例联合验收结论。
