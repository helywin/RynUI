# 加载中的 Button 悬停反馈

加载中的按钮继续更新悬停颜色，点击和键盘激活仍受加载状态约束。回归覆盖旧版 Primary 入口与显式 Solid/Primary 入口。Linux GCC Release 隔离构建的 Button 回归通过。

Linux 私有 Wayland/Vulkan 的真实 Button 回读完成四个动作检查点。最后一步指针悬停在加载按钮上时，四角相对锁定 Chromium 的最大 RGB 通道差从 34 降至 7。四角取样中通道差大于 8 的数量从 106 降至 0。真实点击仍未触发加载按钮的回调。
