# Proposal

## Why

Collapse 与 Segmented 已有原生组件及聚焦验证，用户还需要可直接操作的组合示例。现有 Gallery 已提供滚动、状态回写与身份保留的验收基础。

## What Changes

- 在 LiveSamples 末尾追加受控 Collapse、多开/accordion、保留编辑与独立 extra，以及横纵向/等宽/图标标签 Segmented 的实际操作。
- 更新两项 support overlay、目录、文档与实际 inventory；验证整页滚动后交互、受控回写和既有组件身份。
- 不扩展组件首版 API，不新增动画、动态拓扑或原生平台验收矩阵；不把 Recording 结果当成 GPU/系统 IME 或 Linux 证据。

## Capabilities

### New Capabilities

- `panel-selection-gallery`：保留面板与分段选择组件的可操作参考示例及共同回归。

### Modified Capabilities

无。

## Impact

影响 token Gallery definition、frame tests、support overlay/生成目录及组件说明。复用现有公开 API 和 MSVC windows-msvc Debug Recording fixture，不新增依赖。
