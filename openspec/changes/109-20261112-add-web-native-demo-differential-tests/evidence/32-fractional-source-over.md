# 分数相位的双层透明叠加

- 双层边缘按实际源色分解。覆盖平面使用两层可见贡献之和，保留各自的贡献差异及光栅外的独立检查。锁定 Chromium 软件路径在普通分解不成立时，只有由声明边界、覆盖率和两次整数合成精确得到的字节能通过。没有扩大颜色或贡献门槛。[Skia 软件混合源码](https://raw.githubusercontent.com/google/skia/5eefbe51d17d2e379fa2d7353827e0ccb1e1f601/src/core/SkBlitter_ARGB32.cpp)
- Windows D3D12/DXIL 实际 DPR 1.5 和 Linux 私有 Wayland llvmpipe Vulkan/SPIR-V 实际 DPR 1。留出板的两个透明叠层局部回读通过，贡献误差分别为 0.00967 和 0.00562。实际 Windows 原生绘制次序故障达到 0.25024，高于固定上限 0.05，且实际提交顺序检查失败。
- 44 项定向 Python 测试通过，包括同一低覆盖像素在未知渲染器下失败，以及锁定软件路径上的错误字节失败。该验证仅覆盖叠层，未发布校准 profile。
