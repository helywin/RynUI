# Why

用户实测发现 Gallery 导航悬浮后文字不可辨认、组件子项不可点击，亮暗主题与窗口背景/字体颜色不一致；Password 等控件使用文字代替图标。需要先建立可复用的图标组件，再统一控件、导航和主题行为。

# What Changes

- 锁定官方 Ant Design SVG 图标资源、许可证及哈希；提供离线可重现的内嵌轮廓资源和 typed `Icon`，默认继承上下文颜色与字号。
- Password 使用眼睛/隐藏眼睛，Input clear 使用关闭圆形，Search 默认按钮使用搜索图标；保留原有输入、焦点、键盘和受控行为。
- 增加真正无边框的文字按钮语义，修复导航透明底上普通按钮边框与文字同色的问题。
- 组件目录子项可点击并定位到对应卡片，具有稳定 identity 和选中反馈。
- 统一手动与自动主题切换，背景、正文、导航、图标及辅助操作颜色同步；固定顶栏提供亮暗切换入口。
- 滚动条滑块与轨道提供普通、悬浮、按下/拖动的主题颜色反馈。

# Capabilities

## New Capabilities

- `typed-offline-icons`：离线图标、上下文样式、响应式局部更新及控件接入。
- `gallery-theme-and-entry-navigation`：导航各层级可操作，亮暗主题和交互状态一致。

## Modified Capabilities

无主规格变更；现有 032 滚动和文档布局合同继续保留。

# Impact

影响公开 Icon/Button API、组件与文字场景资源、Input/Search 内部组合、Gallery 导航模型及主题入口。复用现有 FreeType/HarfBuzz、glyph atlas 与 GPU 绘制路径，无新增运行时图形后端。主要风险为轮廓转换精度、DPI 缓存生命周期、图标尺寸改变后的输入命中，以及导航新增交互后的自动验收定位。

本 change 不实现通用 SVG 加载器、全量图标库、双色/旋转图标 API，也不把部分支持标记为完整 Icon 对齐。本机验收 Windows MSVC/D3D12，平台通用合同只测一次；不声明 Linux 实机结果。
