# Proposal

## Why

RynUI 缺少用于少量互斥视图切换的原生 Segmented。提供紧凑、可键盘操作且保持节点身份的共同组件，延续锁定的 Ant Design 6.6.5 基线。

## What Changes

- 增加 typed Props、固定唯一 String key 的选项、响应 label/disabled、typed rich label/icon slots，以及受控/默认值与 onChange。
- 增加自然宽度、block 等宽、横纵排列、三种尺寸与独立 Theme tokens。
- 接入共同 pointer/focus/keyboard、Modal/Spin 输入资格、retained logical scene；完成 MSVC headless 定向验证。
- 首版不包含滑块动画、动态选项拓扑、RTL、平台 accessibility adapter 和完整 Web aliases；不改全局 Gallery 或其他待合并组件。

## Capabilities

### New Capabilities

- `native-segmented`: 固定选项的原生单选分段控件。

### Modified Capabilities

无。

## Impact

新增独立 public header、host、tokens、tests 和 docs；仅最小修改 Theme、WindowComponentServices、公开聚合 header 和 CMake。风险集中在实际 place_child 拉伸、富标签换行、输入边界及回调销毁。没有依赖升级、renderer/scene ABI 或 SDK 变更。
