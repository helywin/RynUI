# SVG 图标视口垂直定位

Icon 文本场景保留实际字体行间距的小数部分，使主 glyph 和各颜色层使用同一基线。固定参考的 SVG 边缘支持域按像素面积与几何范围相交计算，仍检查裁剪外出现的墨迹。

Windows MSVC HEADLESS Debug 的 Icon/Text 定向 CTest 4/4、Python 分层比较 34/34 通过。Windows MSVC Release 隐藏 D3D12 与 Linux GCC Release 独立 Wayland/Vulkan 的定向套件各 7/7 通过。Windows 1.5 倍实际 Button 初始帧中，Loading 图标视口顶部对齐到参考坐标 y=105；该帧仍存在 SVG 覆盖及阴影差异，尚无配对通过结论。
