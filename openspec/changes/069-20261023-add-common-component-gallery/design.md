# Design

## Context

见 proposal。Gallery 已使用长期保留的 state、typed Props/slots 和 Recording frame 测试；新组件已有公共 API 与聚焦合同。

## Goals / Non-Goals

在现有 LiveSamples 末尾追加常用组件示例，保证既有样例索引与状态稳定。不复制组件内部实现，不增加自动演示计时器，不改变渲染或依赖基线。

## Decisions

- 复用 GalleryState 的 Signal 保存受控值，以按钮更新加载、进度、计数和可见性；避免为展示引入轮询与额外后台状态。
- 复用一个响应式 Drawer 切换四个方向，正文放真实编辑控件、Popconfirm 和长内容，页脚关闭后重新打开仍保留草稿。相比四套重复正文，减少挂载规模并直接展示响应式方向切换。
- Tag/Badge、反馈和独立 Popconfirm 使用公开 API；support overlay 只记录实际首版能力，不把完整 Ant Design API 标为已支持。
- 平台通用测试使用 windows-msvc/MSVC、Ninja Multi-Config 和 Recording，覆盖滚动后的真实入口定位与保留内容；各组件原生平台验收不混入本 change。

## Risks / Trade-offs

新增挂载会改变 inventory → 依据实际输出更新断言，保留行为断言。整页测试较慢 → 先运行新场景的聚焦入口，再执行一次完整 Gallery 合同。新增关闭的浮层 → 确认不占普通布局、不创建不可达的可见入口。

## Migration Plan

规划检查后独立英文提交并推送 main；随后实现、验证、记录证据并提交推送。无需旧调用迁移，可独立回滚示例提交。
