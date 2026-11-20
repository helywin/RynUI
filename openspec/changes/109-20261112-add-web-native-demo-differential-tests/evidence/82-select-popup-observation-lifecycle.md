# Select 浮层观测与样例生命周期

Gallery 观测将实际挂载的 Select 弹层登记成独立绘制归属。场景离页后仅生成当前帧回读，不为已释放的样例生成分片或 glyph 证据。

Windows MSVC Release（隐藏 D3D12）、Linux GCC Release（独立 Wayland/Vulkan）的定向套件各 7/7 通过，包含原生观测生命周期测试。真实 Select 用例两平台各采集 10 个检查点、每个检查点 3 个文档分片，包含虚拟列表和异步选项的打开、编辑及完成状态。Windows 打开帧观测到独立弹层的填充、阴影及选项文字。完整配对视觉和语义判定仍待验证。
