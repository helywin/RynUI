# Proposal

## Why

通用和布局分类已完成本轮 Linux 收口，导航及后续分类仍有未实现组件和首版原生功能缺口。用户要求按目录顺序持续实现，让独立 Gallery 页面成为真实可操作的组件说明。

## What Changes

- 先补齐 Anchor、Breadcrumb、Dropdown、Menu、Pagination、Steps、Tabs，再推进数据录入、数据展示、反馈及其他分类。
- 保留已通过的原生功能；按锁定 Ant Design 6.6.5 合同补齐尚缺的 typed API、主题、布局、输入、动态内容和生命周期。
- 为图片、文件操作、异步数据及全局配置提供明确的原生服务边界，不引入浏览器或组件私有 GPU/SDL 路径。
- 每个组件配套实际 Gallery 示例、支持范围、定向自动测试与精简验收；只有实际实现并验证后才提升目录状态。
- 沿用 106 已完成的 Linux 基线，Windows 平台项独立记录，不重复实施或撤回历史已通过项。

## Capabilities

### New Capabilities

- `navigation-components`: 锚点、层级、菜单、分页、步骤与页面切换的完整原生交互。
- `data-entry-components`: 编辑、日期时间、层级选择、文件列表及数据传输的原生输入合同。
- `data-display-components`: 集合、图像、内容展示及引导的原生呈现和交互。
- `feedback-components`: 状态反馈、浮层、通知、加载和水印的原生合同。
- `application-context-components`: 固定定位、应用服务、配置作用域和视觉辅助的原生合同。

### Modified Capabilities

无；主 spec 库当前为空，已有行为继续遵守历史 change 和现有测试。

## Impact

涉及 include/ryn、Core 组件及共享服务、Theme、逻辑 CPU scene、必要的平台适配、Gallery、测试和组件文档。基础 scene ABI v2、112-byte Effect 与 Core 依赖边界保持；Progress 连续渐变与新端帽增加显式 Effect Paint ABI v1，按独立共同逻辑、资源事务、真实 GPU 和组件消费阶段实施，未实现的能力不由研究报告背书。新增资源能力必须走共同资源事务。DOM/CSS/React/URL 路由与浏览器安全策略不移植，已废弃 List 不复活，以 Listy 为目标。风险集中在动态 keyed 内容、浮层/捕获/焦点清理、受控请求、异步取消和媒体资源；依靠分阶段合同与后台 GPU 验证。
