## Purpose

为高度不同的原生卡片和内容提供多列瀑布流布局，根据真实测量结果减少列间空白，响应窗口与内容尺寸变化，保留稳定键对应的组件身份并报告实际列分配。

## ADDED Requirements

### Requirement: 稳定瀑布流排列

Masonry SHALL 支持 typed keyed items、响应式 columns/gutter、可选高度和指定 column、最短列放置、动态尺寸刷新及 onLayoutChange。无效键、列数、尺寸 SHALL 显式拒绝，不产生非有限几何。

#### Scenario: 内容变高与窗口变窄
- **WHEN** 已挂载项目高度变化或窗口跨列数断点
- **THEN** 根据实际测量重新分列，键与内容状态保持，回调仅在分配改变时发出。
