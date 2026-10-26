# Proposal

## Why

Popover、Message/Notification 与 Timeline 已有原生实现和专项检查，但 Gallery 尚不能直接体验它们的业务流程。需要接通真实编辑、提示和进度记录，让已有能力可以从目录直接使用。

## What Changes

- 在旧样例之后追加 Popover 备注编辑、消息/通知操作台及 Timeline 处理记录，不改既有样例函数与 stable ID 相对顺序。
- Popover 展示真实触发、输入/保存、定位/箭头、禁用恢复；通知展示打开、同 key 更新、关闭/清空、位置/状态及富内容编辑保留。
- Timeline 展示正反序、三种位置、loading、隐藏恢复和真实备注编辑，外部恢复按钮保持可操作。
- 四个组件目录直接定位样例；增加 `--overlays-timeline-only` 定向 Recording 流程，覆盖三主题、宽窄有限视口、实际事件、裁剪与 retained 身份。
- 更新四条 support overlay、生成目录和对应 API 文档。完整 Gallery 与原生启动集中在后续批次收尾，不在每个阶段重复。

## Capabilities

### New Capabilities

- `overlay-timeline-gallery`: 原生浮层编辑、消息通知与处理记录的交互 Gallery 合同。

### Modified Capabilities

无；当前主 specs 目录没有已发布 capability。

## Impact

范围为 Gallery definition/生产 anchor、frame tests、Popover/Message/Notification/Timeline 的文档和支持条目、本 change。现有 Core、公开 API、Theme、goldens 与 CMake 不改，不引入依赖。主要风险是窗口层与文档滚动的归属、关闭后的通知生命周期、窄视口实际裁剪和新增样例影响盘点；通过真实目录与输入事件、场景采样、精确 inventory 和基线比较验证。未提供所有组件的原生 GPU/人工视觉、Linux、系统 IME 或 AT 验收。
