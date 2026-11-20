# 合成帧使用实际呈现格式回读

正式校准曾因普通 Input 回读 BGRA8、合成 Button 回读 RGBA8 而中止。合成回读现在复用呈现时的 GPU blit，再从符合实际交换链格式的同尺寸目标下载。记录真实源格式和通道顺序，继续验证源帧、fence 和解码 RGBA。

Windows MSVC Release 隐藏 D3D12、Linux GCC Release 独立 Wayland/Vulkan 的合成、通道与 shader 布局测试各 2/2 通过。两平台各 4 个实际 Button 检查点均记录 BGRA8，解码图像的 SHA256 与修复前的 RGBA8 回读逐帧一致。完整独立校准及三例配对判定继续单独追踪。
