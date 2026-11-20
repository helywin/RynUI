# Button 加载态合成验证

- Windows MSVC Release 后台 D3D12。Button 组件、圆角 shader 合同、paint shader 源依赖与合成回读共 4/4 通过。回读覆盖 1×、1.5×、两种底色、重叠图层和祖先硬裁剪。
- Windows MSVC HEADLESS Release。合成场景与失败回滚 2/2 通过，含实际绘制范围裁剪和能力拒绝。
- 固定官方 Chromium 141 与 RynUI Gallery 的 1.5× 实际回读中，Loading 主按钮内部三个采样点的最大 RGB 通道差由 7 降到 1。加载态现在从同一离屏图层合成背景、阴影、图标与文字。

完整 Button 差分仍有颜色覆盖、边缘与字形差异。Linux GPU 尚未执行这一阶段的回归。
