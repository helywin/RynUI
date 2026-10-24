# Proposal

## Why

原生 Breadcrumb/Steps 核心已独立审查，但 Gallery 目录仍只有参考卡，无法直接体验导航和富内容编辑。需要把固定拓扑、响应式状态和原生子控件组成真实可用样例，并以实际输入和有限视口场景验证。

## What Changes

- 在 Date 样例之后追加 Breadcrumb 动态层级和 Steps 受控流程，保留全部既有 ID、函数体和顺序。
- 目录 Breadcrumb/Steps 使用生产 anchor helper 跳转至新增样例；其他目录行为保持。
- 增加聚焦 Recording 验证、支持 overlay 的目标条目与组件文档证据。
- 不修改 Core、公开 API、Theme、golden、CMake ABI、Time 或 Descriptions；不宣称 GPU、原生窗口、IME、Linux、辅助技术验收。

## Capabilities

### New Capabilities

- `navigation-gallery`: 原生导航组件的交互样例、目录可达性和保留身份验证。

### Modified Capabilities

无。

## Impact

仅 Gallery definition、共享 anchor helper、frame tests、目标 overlay/生成目录、Breadcrumb/Steps 文档及本 change。核心固定拓扑、无 RTL/router 和复杂 Steps 变体的限制继续以 partial 范围呈现。新增 Input 和导航焦点可能暴露旧 fixture 的全局定位假设，必须改用样例所有权而非放宽断言。
