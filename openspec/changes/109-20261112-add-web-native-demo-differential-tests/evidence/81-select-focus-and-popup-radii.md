# Select 焦点轮廓和弹层圆角

按固定 Ant Design 6.6.5 的 Select token 和 dropdown 样式恢复默认半透明焦点轮廓，弹层和选项分别使用大、小圆角。已有圆角覆盖继续适用于两个表面，可单独覆盖选项圆角。五份主题快照由实际构建的主题工具重新生成。

Windows MSVC HEADLESS Debug 的 Select、Theme 定向 CTest 4/4 通过。Windows MSVC Release（隐藏 D3D12）、Linux GCC Release（独立 Wayland/Vulkan）的定向套件各 7/7 通过。Windows 实际打开受控 Select 后，提交的弹层填充半径为 8，选中项为 4。

两平台完成了各 10 个 Select 检查点的采集。完整 Web/native 配对判定仍待验证。
