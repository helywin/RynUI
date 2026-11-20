# Input 下划线与空 affix 布局

- 下划线变体的背景覆盖完整边框盒。真实非空前缀和后缀继续占用间距，空 slot 的测量宽度不挤占编辑区。
- Windows MSVC HEADLESS Release 定向 Input component、variant、actions 和 Select component 4/4。Windows D3D12 与 Linux 私有 Wayland/Vulkan Release 同期定向 CTest 各 9/9。
- Windows 1.5× 的 Select 下划线样例实采背景高度 31→32，首字逻辑起点 15→12，分别与 Chromium 的 32 和 12 对齐。保留 1 逻辑像素底边。该观测仅对应上述部件几何。
