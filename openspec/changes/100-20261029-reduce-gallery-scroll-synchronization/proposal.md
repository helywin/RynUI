# Proposal

## Why

最新 Gallery 在 Windows Release 自动滚动中平均 15.287 ms、p95 19.528 ms，仍不能满足高刷使用。大量屏幕外 Input 的不可见文字层继续同步，目录锚点每帧重复扫描全部 Text；需要保留交互合同并减少纯滚动的重复工作。

## What Changes

- 不可见 Input 字形层使用已有 culled 同步路径，恢复显示时保留真实文字、选择、preedit 与完整代际。
- Gallery 仅在布局变化时重新求目录锚点，普通位移复用有效结果。
- 滚动验收从采样开始计数，仍拒绝采样期间的额外 reconciliation。
- 以相同 Release 原生自动滚动复测平均、p95、CPU 分段和资源上传量；不以 Debug/Release 差异冒充优化收益。

## Capabilities

### New Capabilities

- `scroll-scene-efficiency`: 保留文字与导航行为的纯滚动同步、裁剪和测量边界。

### Modified Capabilities

无。

## Impact

限 Input 字形同步、Gallery 目录与采样、对应回归和证据；不增加公开 API、组件功能、字体系统或 renderer ABI。风险是恢复可见时丢字形、锚点缓存失效或测量漏记，须分别回归。原生验收仅本机 Windows，Linux/IME/AT 不在本次声明中。
