# Button 圆角描边和自定义加载图标

普通连续描边在完整背景上绘制独立圆角环，保持原生布局、命中区域和已有的 Compact 与虚线路径。使用自定义加载图标时，内置加载点不参与绘制。

使用 Windows MSVC Release 和 Linux GCC Release 的编译与链接参数对相关目标做隔离构建，两个平台的 Button 组件回归均通过。实际隐藏 D3D12 和私有 Wayland/Vulkan 回读各完成四个 Button 检查点。与锁定 Chromium 对比，键盘焦点状态的默认按钮四角最大 RGB 通道差在 Windows 150% 下从 45 降至 2，在 Linux 100% 下从 54 降至 3。此结论限于回读中的圆角区域，整套 pilot 继续验收。
