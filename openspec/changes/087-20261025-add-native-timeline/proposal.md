# Proposal

## Why

RynUI 需要可承载真实编辑控件的原生时间轴，用于展示事件和处理过程。现有 Steps 的导航语义不能替代 Timeline 的双侧事件布局与反序展示。

## What Changes

- 新增 vertical Timeline，支持 Start/End/Alternate、item placement、reverse、固定 keyed items 与响应式 visibility。
- 使用现代 title/content/icon/loading 字段、typed 富 slots、响应式文本与 typed tone；独立完整 Theme chain。
- 保持编辑器、草稿、refs、子组件身份；反序同步 Tab 顺序，隐藏清理输入与 popup。
- 新增模型/API/Theme/实际场景与组合测试、中文 docs/timeline.md 和可编译示例。
- 非目标：horizontal、titleSpan、RTL、Web AT、动画、动态增删/任意排序、deprecated pending/pendingDot/children/dot/label/position、任意 style。loading 为静态图标与虚线轨迹。

## Capabilities

### New Capabilities

- `native-timeline`: 原生时间轴的布局、保留身份、主题、裁剪与交互组合合同。

### Modified Capabilities

无。

## Impact

新增公开 Timeline 与 token 头、Core host/model、window services 注册与 Theme/runtime/snapshot 集成；不新增依赖，不改变 renderer、logical scene v3、GPU ABI 或全局 Gallery/完成记录。风险集中在极窄裁剪、反序焦点顺序、同步回调卸载与 Theme 原子拒绝，通过 headless 实际 glyph/surface/hit 和生命周期验证收口。
