# DirectWrite 黑色字形的材质 alpha

固定 DirectWrite 灰度路径先在整数 A8 域合成材质 alpha 和已传递的掩码，再交给帧缓冲混合。独立字体见证使用同一量纲，继续拒绝应有墨迹的纯背景。原有全彩及 Fontations 传递路径保持各自的混合合同。

Windows MSVC Release / D3D12 和 Linux GCC Release / 私有 Wayland Vulkan 的 glyph shader、合成布局及实际回读各 3/3 通过。隐藏回读使用独立 A8 输入检查 RGB87、35、31 及空白边缘。分层比较测试 39/39 通过。

同一 Chromium 141 参考及原 Windows 严格阈值的专项重放中，Input 初始、接受和拒绝检查点的报告分别从 292、348、348 条降至 5、56、56 条。余项仍包含 SVG 轮廓及对焦边缘。最终着色器构建的三张原生截图与该重放输入逐字节一致。
