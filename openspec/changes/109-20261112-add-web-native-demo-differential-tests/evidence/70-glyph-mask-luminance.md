# 彩色字形覆盖率验证

- Windows MSVC Release 与 Linux GCC Release 各通过 5/5 项定向 CTest，覆盖字体运行时、glyph scene、Input paint 快照及 shader 合同。分层比较器通过 30/30 项 Python 测试。
- Windows D3D12 1.5× 和 Linux 私有 Wayland/Vulkan 1× 的 Input 三个检查点均完成后台回读。Windows 的独立字形遮罩显示，示例中“前”的同一边缘像素在双方覆盖率原始字节均为 128。原生 RGB 从 (252, 203, 106) 校正到 (252, 202, 104)，固定 Chromium 参考为 (252, 202, 103)。
- Windows 与 Linux 的三个检查点在诊断用固定上限下均无差异报告。独立控制校准及正式 paired 结果仍需按集成版本重跑。
