# Vulkan SVG 图标四分之一透明度的精确复算

- Linux GCC Release，私有 Wayland/Vulkan、DPR 1。复用正式三例运行中 Input 的原始 Web/native 帧，按锁定 SVG 路径及实际提交的字形位置独立生成覆盖。清除图标 144 个像素均符合对应结果，其中 13 个恰落在半色阶，Chromium 和 Vulkan 相差一个灰阶。
- 同一现场回读、同一 Linux profile 重放 `initial` 检查点。启用复算前 4 项诊断失败，启用后仅剩 1 项圆角边框轮廓差异。非官方路径、不同位置、非白底、颜色偏差及错字形资源继续保留原判断。分层和校准测试共 68 项通过。
- 本次仅覆盖正式回读的局部复算。完整 Linux 三例和 meta 未通过。
