# Spec Delta

## Purpose

为框架使用者和后端作者提供显式、可验证的宿主与 renderer 选择边界，使共同组件和文本场景可以独立于桌面窗口、GPU 驱动、shader 工具和系统字体配置构建。

## ADDED Requirements

### Requirement: Headless dependency isolation
框架 SHALL 提供 HEADLESS + Recording 配置，构建真实 UI Core 与共同资源路径，且不查找、下载、生成或链接 SDL、原生 GPU shader 工具和系统字体适配器。

#### Scenario: Unavailable native dependencies
- **WHEN** 使用 HEADLESS preset 且提供无效的原生 shader 工具路径
- **THEN** 配置和共同组件/Recording tests 构建通过，实际构建图与编译记录不包含上述原生依赖

### Requirement: Explicit supported selection
框架 MUST 显式拒绝未知宿主、renderer 和不支持的组合，并对默认 SDL + SDL_GPU 保持可构建的桌面行为。

#### Scenario: Invalid pair
- **WHEN** 选择 HEADLESS + SDL_GPU 或未知 backend
- **THEN** 配置阶段报告具体组合错误

### Requirement: Shared core dependency boundary
共同组件、运行时、布局、文本、graphics 和 renderer 合同 MUST 不包含 backend/OS SDK headers，也不得传递链接原生后端。

#### Scenario: Internal backend leak
- **WHEN** 共同层引入 SDL/native backend header 或链接依赖
- **THEN** 架构验收失败并指出违规文件或 target
