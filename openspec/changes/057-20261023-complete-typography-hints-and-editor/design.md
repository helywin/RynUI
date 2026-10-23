# Design

## Context

动机见 proposal。034 已有保留编辑/操作子树、独立剪贴板、受控草稿和 deferred focus；040/043 提供 Tooltip，053 提供 TextArea。Typography 的三个提示字段尚未使用，editor 挂载处仍是 Input。

## Goals / Non-Goals

**Goals:** 复用共用组件；更新文案、截断状态和编辑配置时保持 identity；延续按需帧与 logical scene v3 边界。

**Non-Goals:** 不新增浮层管理器、文本模型或平台分支，不更改 Theme 视觉入口，不扩大到 Web API 完整兼容或历史平台验收。

## Decisions

1. 在现有复制/编辑 action 外预建 Tooltip，以 Signal 传递普通/成功文案和可用性；空文案通过共用 Tooltip 的标题合同关闭。将文本的 Tooltip 包装限制在 display 分支，使用独立的布局根和实际 Text id，避免末尾 action 的 hover 命中全文提示。保留原有 action id 与 Tab 顺序，不向 wrapper 增加 Tab 入口。备选的组件私有浮层会重复计时、定位与清理，故不采用。
2. 省略提示可用性依据实际 text-state truncated、expanded、editing 和 disabled 更新；布局宽度变化后同步，不能仅根据 ellipsis 配置判断。计时与位置仍由 Tooltip 负责。
3. `TypographyEditable` 尾部增加 `TextAreaAutoSize auto_size{true}`，兼容既有聚合初始化。预建 rows=1、resize=None 的 TextArea，绑定 reactive autoSize。Input 宿主仅为配置了内部 Typography commit 的编辑器优先处理无修饰 Enter；Shift+Enter 走既有多行插入，Ctrl+Enter 保留 TextArea 的提交入口。IME 处理先于提交。
4. 保留 RynUI 原文和换行，不复制 Web 编辑器删除换行和 trim 的行为。Ant Design 6.6.5 的 Editable 使用 rows=1/autoSize TextArea，当前上游却在 onChange 删除换行；原生多行支持是收尾清单要求的桌面映射，文档明确此边界。其他 commit/cancel/controlled 语义沿用 034。
5. Common 在 Windows `windows-msvc-headless` Debug/Release 实际构建并运行完整 CTest；Windows native 使用 `windows-msvc` Debug/Release 的 MSVC x64、Ninja Multi-Config、D3D12/DXIL 窗口验证提示、编辑、DPI、resize/idle/dispose；Linux native 单列 pending，不用 WSLg/Windows 替代。

参考：Context7 `/ant-design/ant-design` Typography 文档；固定版本源码 [Base](https://github.com/ant-design/ant-design/blob/6.6.5/components/typography/Base/index.tsx)、[Editable](https://github.com/ant-design/ant-design/blob/6.6.5/components/typography/Editable.tsx)。当前文档只辅助发现 API，版本行为以固定源码与本地合同为准。

## Risks / Trade-offs

- Tooltip 包装改变组件层次和 interaction 数量 → 测试按真实 action/component 查找，不依赖 declaration 下标，验证顺序与布局保持。
- 在布局中同步提示可能引起额外失效 → 仅发送发生变化的 Signal，验证 settle 后无持续帧/截止时间。
- 多行提交与普通 TextArea 键盘冲突 → 内部 commit 特判覆盖限定到 Typography editor，补普通 TextArea Enter 回归。

## Migration Plan

按提示、多行编辑、Gallery 与原生验收分阶段验证并提交；保留历史证据，不自动 push/archive。源码回退可撤销相应阶段提交，不迁移持久数据。
