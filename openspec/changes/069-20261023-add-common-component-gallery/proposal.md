# Proposal

## Why

已合入的 Drawer、Popconfirm、Tag/Badge 和反馈组件缺少实际 Gallery 示例，使用者尚不能从示例程序直接体验其主要交互。补齐入口和组合场景，让常用组件能直接试用。

## What Changes

- 在现有 LiveSamples 后追加四边 Drawer、受控 Popconfirm、可关闭/可选择 Tag、计数/圆点/状态 Badge、Alert、Spin 和三种 Progress。
- 示例展示保留编辑、加载屏蔽、受控写回和响应式更新，并保持原有样例和导航。
- 更新 support overlay/catalog 与用户文档，记录首版支持范围及未实现功能。
- 用 Recording Gallery 合同验证整页滚动后的打开、编辑、关闭和状态保留。

## Capabilities

### New Capabilities

- `common-component-gallery`: 常用状态、反馈和浮层组件的实际交互示例与保留状态合同。

### Modified Capabilities

无。

## Impact

局部修改 examples/token_gallery、Gallery tests、support overlay/generated catalog 和文档；不改变公开 API、renderer、Token 基线或第三方依赖。风险是新增控件影响既有 inventory 和整页滚动坐标，使用实际挂载数量及交互验证。此 change 仅做平台通用集成，不代替各组件独立原生 GPU/DPI、Linux 或人工系统 IME 验收。
