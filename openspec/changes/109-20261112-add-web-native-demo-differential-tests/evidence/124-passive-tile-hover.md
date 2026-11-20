# 文档相机与悬停

在记录主帧之后，两侧仅在指针未按住时将私有测试指针移到视口外，再遍历文档相机。相机恢复后还原指针位置。应用的真实动作及悬停仍由主帧记录。

Windows MSVC HEADLESS Release 的 native 输入、native 观测和 Select 回归共 3/3 通过。D3D12 与 Chromium 的十个主帧及其状态分别保持不变，回调轨迹一致。相机帧的下划线颜色像素 10/10 与 Chromium 一致，包括此前受滚动诱发悬停影响的 `async-delivered` 检查点。
