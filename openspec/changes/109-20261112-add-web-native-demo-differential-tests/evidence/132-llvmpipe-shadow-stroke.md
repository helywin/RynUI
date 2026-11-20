# llvmpipe 圆角硬阴影与描边复算

- Linux GCC Release，私有 Wayland、Mesa llvmpipe Vulkan、BGRA8、DPR 1 的现场回读。针对整数网格、1 物理像素描边，分别独立复算 Chromium 的半描边内缩填充与描边，以及 Vulkan 的 R8 遮罩和分项整数 source-over。受控输入中的光标继续由独立光标检查负责。
- Windows MSVC `windows-msvc-release` 构建校准工具。对同一批 Linux Input 原始帧使用原 profile 重放，初始、接受、拒绝由 `0/32/32` 项差异降为 `0/0/0`。连续颜色反推的边界值只有在两侧原始 RGB8 精确等于各自独立复算时才使用该证明。分层及形状见证测试共 58 项通过。
- 这些是用真实原始帧获得的 Input 回读诊断。完整 Linux 三例、原生新版本全链和 meta 仍待验证。
