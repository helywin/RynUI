# 无边框 Select 的编辑器边距

固定的 Ant Design 6.6.5 Select 保留透明边框占用的内联空间。原生 Select 的嵌入编辑器保留同一空间，独立的无边框 Input 延续原有布局。组件回归同时比较三种入口的实际 viewport。

MSVC HEADLESS Debug 和 Release 各通过 2/2 项定向 CTest，Linux GCC Release 通过 2/2 项。1× 私有 Wayland/Vulkan 完成 Select 的 10 个原生检查点。原先在 `(17.5, 701.5)` 提前出现的文字墨迹由 RGB 105 恢复为 255，与 Chromium 在该坐标相同。整例差分仍在验证。
