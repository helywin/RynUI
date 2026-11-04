# Spec Delta

## Purpose

规定 RynUI 导航分类可供原生桌面应用依赖的组件行为，沿用锁定 Ant Design 6.6.5 的设计合同，并将功能实现、平台集成与独立 Gallery 示例分别验证，避免以目录占位替代真实交互。

## ADDED Requirements

### Requirement: Anchor 原生合同
系统 SHALL 为 Anchor 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。提供显式原生滚动目标、键控层级锚点、横纵方向、活动项跟踪、偏移与边界、点击及变化事件、固定定位与平滑滚动；不得模拟浏览器 hash 导航。

#### Scenario: Anchor 正常交互及状态更新
- **WHEN** 应用挂载 Anchor 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Anchor 禁用和生命周期
- **WHEN** Anchor 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Breadcrumb 原生合同
系统 SHALL 为 Breadcrumb 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。支持动态键控条目、保留富标题和图标、分隔符、内置菜单、换行与 RTL；激活通过原生事件表达，移除项清理焦点及浮层。

#### Scenario: Breadcrumb 正常交互及状态更新
- **WHEN** 应用挂载 Breadcrumb 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Breadcrumb 禁用和生命周期
- **WHEN** Breadcrumb 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Dropdown 原生合同
系统 SHALL 为 Dropdown 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。补齐 hover 及组合触发、分离按钮、自定义保留 popup、箭头/定位/溢出、开合事件来源和受控请求；支持子浮层与焦点恢复。

#### Scenario: Dropdown 正常交互及状态更新
- **WHEN** 应用挂载 Dropdown 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Dropdown 禁用和生命周期
- **WHEN** Dropdown 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Menu 原生合同
系统 SHALL 为 Menu 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。支持动态键控结构、Horizontal/Vertical/Inline、横向溢出菜单、纵向子菜单、折叠、主题、RTL、多选与完整键盘导航；保留未变项身份。

#### Scenario: Menu 正常交互及状态更新
- **WHEN** 应用挂载 Menu 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Menu 禁用和生命周期
- **WHEN** Menu 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Pagination 原生合同
系统 SHALL 为 Pagination 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。补齐页大小选择、快速跳页、simple、响应式、对齐、总数与 typed item 渲染；正确处理非法输入、总数收缩和受控拒绝。

#### Scenario: Pagination 正常交互及状态更新
- **WHEN** 应用挂载 Pagination 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Pagination 禁用和生命周期
- **WHEN** Pagination 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Steps 原生合同
系统 SHALL 为 Steps 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。补齐动态条目、dot/inline/navigation/panel、percent/maxCount、横纵/响应式/RTL、富内容和有限动画；禁用及移除项不得继续交互。

#### Scenario: Steps 正常交互及状态更新
- **WHEN** 应用挂载 Steps 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Steps 禁用和生命周期
- **WHEN** Steps 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Tabs 原生合同
系统 SHALL 为 Tabs 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。支持动态键控新增/删除/重排、editable-card、lazy/destroyOnHidden、四方向/RTL、富标签和 extra slots、溢出菜单及 indicator/有限动画；保留未变面板草稿并正确清理隐藏输入。

#### Scenario: Tabs 正常交互及状态更新
- **WHEN** 应用挂载 Tabs 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Tabs 禁用和生命周期
- **WHEN** Tabs 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 独立页面和真实支持状态
系统 SHALL 让每个有效组件目录项拥有独立说明和真实示例，仅在合同实现并通过相应自动验证后提升支持状态。平台未验证不得伪装为通过，既有通过证据不得因切换机器而回退。

#### Scenario: 完成单个组件阶段
- **WHEN** 一个组件的新增功能及受影响自动检查完成
- **THEN** 页面、支持清单和精简验收同步更新；原始运行产物仅保存在 out，尚未实现的行为仍明确列出。
