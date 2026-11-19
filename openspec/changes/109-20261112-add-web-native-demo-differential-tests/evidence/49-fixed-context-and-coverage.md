# 固定样例原点与控制板覆盖

- 差分场景以实际挂载的样例内容容器为相对原点。Windows 和 Linux 的原生 Button、Input、Select 共 17 个语义检查点通过。Windows 与固定 Chromium 的 Button、Input 目标相对几何一致。Select 的 RTL 富前缀、部分弹层几何及长样例分块捕获仍待验证。
- 固定原生截图模式使用锁定 Skia 从实际提交的 Quad 与 RoundedEffect 生成覆盖，沿用原生 GPU 合成。固定 Chromium 对照使用显式光栅块尺寸，分数裁剪控制按声明的半开物理像素边界放置。默认原生呈现方式与正式参考样例源保持原样。
- Linux `linux-gcc-release` 私有 Wayland/Vulkan 与固定 Chromium 在 DPR 1 检查训练、留出各 312 个样本，两组全部在原有硬上限内。Windows `windows-msvc-release` hidden D3D12 与固定 Chromium 在 DPR 1.5 检查同一控制矩阵，训练板在硬上限内。留出板有两个低对比度矩形角像素相差一个 RGB8 色阶，局部覆盖误差为 0.0625，上限为 0.05。两平台阴影均在原有上限内。
- Windows `windows-msvc-headless` Debug 和 Release 各通过 3/3 定向 CTest。控制、分层和区域 Python 测试通过 42/42。本轮记录单次正控制和原生语义检查结果，完整重复校准及成对 pilot 尚未通过。
