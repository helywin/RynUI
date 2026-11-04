# Spec Delta

## Purpose

规定 RynUI 反馈分类可供原生桌面应用依赖的组件行为，沿用锁定 Ant Design 6.6.5 的设计合同，并将功能实现、平台集成与独立 Gallery 示例分别验证，避免以目录占位替代真实交互。

## ADDED Requirements

### Requirement: Alert 原生合同
系统 SHALL 为 Alert 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Alert 正常交互及状态更新
- **WHEN** 应用挂载 Alert 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Alert 禁用和生命周期
- **WHEN** Alert 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Drawer 原生合同
系统 SHALL 为 Drawer 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Drawer 正常交互及状态更新
- **WHEN** 应用挂载 Drawer 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Drawer 禁用和生命周期
- **WHEN** Drawer 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Message 原生合同
系统 SHALL 为 Message 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Message 正常交互及状态更新
- **WHEN** 应用挂载 Message 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Message 禁用和生命周期
- **WHEN** Message 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Modal 原生合同
系统 SHALL 为 Modal 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Modal 正常交互及状态更新
- **WHEN** 应用挂载 Modal 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Modal 禁用和生命周期
- **WHEN** Modal 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Notification 原生合同
系统 SHALL 为 Notification 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Notification 正常交互及状态更新
- **WHEN** 应用挂载 Notification 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Notification 禁用和生命周期
- **WHEN** Notification 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Popconfirm 原生合同
系统 SHALL 为 Popconfirm 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Popconfirm 正常交互及状态更新
- **WHEN** 应用挂载 Popconfirm 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Popconfirm 禁用和生命周期
- **WHEN** Popconfirm 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Progress 原生合同
系统 SHALL 为 Progress 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Progress 正常交互及状态更新
- **WHEN** 应用挂载 Progress 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Progress 禁用和生命周期
- **WHEN** Progress 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Result 原生合同
系统 SHALL 为 Result 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Result 正常交互及状态更新
- **WHEN** 应用挂载 Result 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Result 禁用和生命周期
- **WHEN** Result 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Skeleton 原生合同
系统 SHALL 为 Skeleton 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Skeleton 正常交互及状态更新
- **WHEN** 应用挂载 Skeleton 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Skeleton 禁用和生命周期
- **WHEN** Skeleton 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Spin 原生合同
系统 SHALL 为 Spin 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Spin 正常交互及状态更新
- **WHEN** 应用挂载 Spin 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Spin 禁用和生命周期
- **WHEN** Spin 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Watermark 原生合同
系统 SHALL 为 Watermark 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。文字/图像重复水印、旋转/间距/偏移、主题及内容尺寸变化。

#### Scenario: Watermark 正常交互及状态更新
- **WHEN** 应用挂载 Watermark 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Watermark 禁用和生命周期
- **WHEN** Watermark 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 独立页面和真实支持状态
系统 SHALL 让每个有效组件目录项拥有独立说明和真实示例，仅在合同实现并通过相应自动验证后提升支持状态。平台未验证不得伪装为通过，既有通过证据不得因切换机器而回退。

#### Scenario: 完成单个组件阶段
- **WHEN** 一个组件的新增功能及受影响自动检查完成
- **THEN** 页面、支持清单和精简验收同步更新；原始运行产物仅保存在 out，尚未实现的行为仍明确列出。
