# Gallery Select 清空操作回归

- 页面测试先移动真实指针，等清空按钮接替默认后缀，再按变换后的屏幕坐标点击。测试仍检查清空后离页再返回时保持空值。
- Windows `windows-msvc-headless-release` 构建 `rynui_gallery_sample_page_tests`，`rynui.gallery_sample_page` 1/1 通过。相同测试动作在集成候选的 HEADLESS Debug 路径也通过，含输入与观测的定向组共 5/5。
- 本次结果覆盖真实原生输入路径的 CPU 回归。Windows D3D12 与 Linux 私有 Wayland/Vulkan 的配对 verdict 仍由各自流程生成。
