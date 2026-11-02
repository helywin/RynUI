# Proposal

## Why

最新 Gallery 在 Windows Release 自动滚动中平均 15.287 ms、p95 19.528 ms，仍不能满足高刷使用。大量屏幕外 Input 的不可见文字层继续同步，目录锚点每帧重复扫描全部 Text；需要保留交互合同并减少纯滚动的重复工作。

## What Changes

- 不可见 Input 字形层使用已有 culled 同步路径，恢复显示时保留真实文字、选择、preedit 与完整代际。
- Gallery 仅在布局变化时重新求目录锚点，普通位移复用有效结果。
- 滚动验收从采样开始计数，仍拒绝采样期间的额外 reconciliation。
- 跳过透明 Button loading 分段的几何更新，合并 Gallery 的有界 Quad 上传并复用 Glyph buffer 增长容量；保留原数据、排序、恢复和失败重试合同。
- 以相同 Release 原生自动滚动复测平均、p95、CPU 分段和资源上传量；不以 Debug/Release 差异冒充优化收益。
- 2026-11-02 补充 Linux 滚轮路径回归：几何落空时提前拒绝命中记录，避免重复遍历屏蔽祖先；测量实际组件滚轮分发，并保留用户手动流畅度确认。

## Capabilities

### New Capabilities

- `scroll-scene-efficiency`: 保留文字与导航行为的纯滚动同步、裁剪和测量边界。

### Modified Capabilities

无。

## Impact

限 Input/Button 同步、renderer/common 的 Quad/Glyph 资源同步、Gallery 目录与采样、hit-test 几何快速拒绝、对应回归和证据；不增加公开 API、组件功能、字体系统或 renderer ABI。风险是恢复可见时丢字形、锚点缓存失效、上传清洁字节失效或测量漏记，须分别回归。既有 Windows 原生结果保持，新增 Linux 自动滚轮结果单独记录；用户流畅度确认、系统 IME 与 AT 不由自动结果代替。
