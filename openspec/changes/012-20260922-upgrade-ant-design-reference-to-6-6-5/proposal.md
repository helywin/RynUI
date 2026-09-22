# Proposal

## Why

RynUI 当前设计参考、离线目录、Token 生成物及 Button/Input 合同共同基于 Ant Design 6.5.0，而用户已决定全项目采用当前最新正式版。若只让新组件引用新版，会造成同一 Theme 与 Gallery 中的组件使用不同基线；因此需要独立评估并迁移到 2026-09-22 核实的 Ant Design 6.6.5。

## What Changes

- 建立 6.5.0 → 6.6.5 的官方来源、组件分类、Design Token、状态视觉和 API 差异清单；以实际 release tag/commit 和 source SHA 固化本次升级输入，后续新版本另行显式升级，不在构建时自动联网漂移。
- 将当前生效的 Ant Design source manifest、组件支持 overlay、Token catalog/golden、生成器输出、Gallery 内容、Theme/Component Token、Button/Input 参考合同与相关测试统一迁移至 6.6.5；重新计算分类与条目数，差异不存在的项目记录等价证据，不机械改数值。
- 更新 README、架构与生成文档的“当前参考版本”；保留旧 change 的历史任务和验收记录，不把旧截图、退出码或未完成平台验收改写成新版通过。
- 复核在途 011 的 Switch/Checkbox 规划，使其以升级后的统一基线实施；本 change 不借版本升级之名提前实现 Switch/Checkbox 或关闭 008 的未完验收。

## Capabilities

### New Capabilities

- `ant-design-reference-baseline`: 统一、可复现的全项目设计参考版本及跨 manifest、Token、组件、Gallery、文档和验收证据的一致性合同。

### Modified Capabilities

无；当前 `openspec/specs/` 尚无已同步主规格。既有在途 change 的历史 delta/evidence 按原版本保留，迁移行为由本 change 的新合同覆盖。

## Impact

- 影响 `gallery/ant-design/`、`design-tokens/ant-design/`、离线生成工具及其 checked-in 输出、Theme/Token 和 Button/Input/Gallery 的 reference source contract、示例、测试、README 与 `docs/architecture.md`。
- 不新增 React、CSS-in-JS 或运行时网络依赖；第三方 source/version/SHA/license 仍按仓库显式锁定规则保存。
- 迁移后的平台通用合同在一个正式 preset 验证，涉及 Win32/D3D12/DXIL 与原生 Wayland/Vulkan/SPIR-V 的实际视觉差异分别验收；旧平台 evidence 只证明旧版本，不自动转正。
