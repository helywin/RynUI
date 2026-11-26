# Select 虚拟列表滚动条

- 按锁定 Ant Design 6.6.5 的 `@rc-component/virtual-list` 1.6.0 实现竖向滚动条。仅在启用虚拟列表且视口存在溢出时显示。RTL 按左边缘定位，LTR 按右边缘定位。拖动、点击轨道和滚轮共用同一滚动位置和原有回调。
- `windows-msvc-headless-debug` 和 `windows-msvc-headless-release` 分别构建并执行 `rynui.portable.select_{component,theme,public_api}`，各 3/3 通过。覆盖真实命中顺序、拖动捕获、方向切换时的节点保留、无滚动条的非虚拟路径和三秒后绘制、命中及截止时间的清理。
- `windows-msvc-release` 构建 Gallery，在隐藏 D3D12 1.5× 路径采集 Select，十个检查点均完成且原生语义断言通过。RTL 虚拟弹层的实际拇指与锁定 Chromium 均为 8×20 逻辑像素，位置一致。原缺失区域的原生灰度从 245 变为 123，Chromium 同一点为 122。该证据覆盖滚动条和原生交互，整例 paired verdict 继续复验。
