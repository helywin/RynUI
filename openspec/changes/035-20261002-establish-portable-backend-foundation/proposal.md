# Proposal

## Why

当前 UI Core 已隔离公开 SDL 类型，但配置仍强制解析 SDL、shader 工具和系统字体，共同场景资源仍位于 SDL 目录，GPU 生命周期与宿主绑定。先建立可实际编译、可验证的后端边界，避免后续组件与功能继续依赖单一桌面实现。

## What Changes

- 增加显式宿主/renderer 选择和 HEADLESS + Recording 构建；该配置不解析 SDL、shader 工具、原生系统字体。
- 提取 renderer 合同、共同场景资源和上传事务；SDL GPU 与 Recording 复用同一场景及资源同步路径，失败保留可重试的 CPU 数据。
- 拆分 SDL 宿主与 GPU 绑定的所有权，GPU 初始化失败后宿主服务仍可用。
- 为现有帧循环增加非阻塞 tick 与呈现挂起/恢复合同，供后续 callback 宿主复用。
- 固定现有 packed scene ABI v1，增加 Core 内部依赖守卫和真实无 SDL 编译验收。
- 更新研究结论、正式架构和 agent 约束；不实现 Android、iOS、Web 或第二个真实 GPU renderer。

## Capabilities

### New Capabilities

- `backend-selection`: 独立构建 UI Core、共同 renderer 合同和已选择的后端，拒绝不支持的组合。
- `scene-renderer-contract`: 共同上传事务、真实 Recording 数据验证、资源所有权和设备代际。
- `host-frame-pump`: 宿主与 GPU 生命周期隔离，以及非阻塞调度、挂起呈现和恢复。

### Modified Capabilities

无。`openspec list --specs` 当前无已归档 capability；本 change 不修改组件的公开行为。

## Impact

涉及 CMake 配置/preset、`src/platform/sdl`、`src/renderer`、帧调度、示例接入、测试与架构文档。FreeType、HarfBuzz、utf8proc 保留为共同依赖，字体字节加载复用现有 API。内部后端 API 可调整，公开 typed Props/slots 与组件语义保持兼容。Windows 原生回归与 Linux 原生验收独立记录；Recording 结果不代表 GPU 视觉或新平台支持。
