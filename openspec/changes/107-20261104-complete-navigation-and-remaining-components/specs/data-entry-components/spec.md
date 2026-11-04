# Spec Delta

## Purpose

规定 RynUI 数据录入分类可供原生桌面应用依赖的组件行为，沿用锁定 Ant Design 6.6.5 的设计合同，并将功能实现、平台集成与独立 Gallery 示例分别验证，避免以目录占位替代真实交互。

## ADDED Requirements

### Requirement: AutoComplete 原生合同
系统 SHALL 为 AutoComplete 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: AutoComplete 正常交互及状态更新
- **WHEN** 应用挂载 AutoComplete 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: AutoComplete 禁用和生命周期
- **WHEN** AutoComplete 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Cascader 原生合同
系统 SHALL 为 Cascader 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。多列层级单选/多选、搜索、动态键控选项、异步加载和禁用边界。

#### Scenario: Cascader 正常交互及状态更新
- **WHEN** 应用挂载 Cascader 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Cascader 禁用和生命周期
- **WHEN** Cascader 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Checkbox 原生合同
系统 SHALL 为 Checkbox 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Checkbox 正常交互及状态更新
- **WHEN** 应用挂载 Checkbox 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Checkbox 禁用和生命周期
- **WHEN** Checkbox 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: ColorPicker 原生合同
系统 SHALL 为 ColorPicker 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。typed 色值、RGB/HSB/HEX 与 alpha、面板/预设、渐变及受控交互。

#### Scenario: ColorPicker 正常交互及状态更新
- **WHEN** 应用挂载 ColorPicker 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: ColorPicker 禁用和生命周期
- **WHEN** ColorPicker 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: DatePicker 原生合同
系统 SHALL 为 DatePicker 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: DatePicker 正常交互及状态更新
- **WHEN** 应用挂载 DatePicker 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: DatePicker 禁用和生命周期
- **WHEN** DatePicker 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Form 原生合同
系统 SHALL 为 Form 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Form 正常交互及状态更新
- **WHEN** 应用挂载 Form 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Form 禁用和生命周期
- **WHEN** Form 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Input 原生合同
系统 SHALL 为 Input 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Input 正常交互及状态更新
- **WHEN** 应用挂载 Input 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Input 禁用和生命周期
- **WHEN** Input 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: InputNumber 原生合同
系统 SHALL 为 InputNumber 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: InputNumber 正常交互及状态更新
- **WHEN** 应用挂载 InputNumber 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: InputNumber 禁用和生命周期
- **WHEN** InputNumber 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Mentions 原生合同
系统 SHALL 为 Mentions 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。Unicode 编辑、触发前缀/搜索、键盘建议、异步更新与 Form。

#### Scenario: Mentions 正常交互及状态更新
- **WHEN** 应用挂载 Mentions 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Mentions 禁用和生命周期
- **WHEN** Mentions 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Radio 原生合同
系统 SHALL 为 Radio 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Radio 正常交互及状态更新
- **WHEN** 应用挂载 Radio 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Radio 禁用和生命周期
- **WHEN** Radio 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Rate 原生合同
系统 SHALL 为 Rate 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Rate 正常交互及状态更新
- **WHEN** 应用挂载 Rate 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Rate 禁用和生命周期
- **WHEN** Rate 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Select 原生合同
系统 SHALL 为 Select 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Select 正常交互及状态更新
- **WHEN** 应用挂载 Select 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Select 禁用和生命周期
- **WHEN** Select 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Slider 原生合同
系统 SHALL 为 Slider 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Slider 正常交互及状态更新
- **WHEN** 应用挂载 Slider 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Slider 禁用和生命周期
- **WHEN** Slider 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Switch 原生合同
系统 SHALL 为 Switch 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Switch 正常交互及状态更新
- **WHEN** 应用挂载 Switch 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Switch 禁用和生命周期
- **WHEN** Switch 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: TimePicker 原生合同
系统 SHALL 为 TimePicker 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: TimePicker 正常交互及状态更新
- **WHEN** 应用挂载 TimePicker 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: TimePicker 禁用和生命周期
- **WHEN** TimePicker 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Transfer 原生合同
系统 SHALL 为 Transfer 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。双列表搜索、选择、方向移动、分页、禁用及动态键控数据。

#### Scenario: Transfer 正常交互及状态更新
- **WHEN** 应用挂载 Transfer 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Transfer 禁用和生命周期
- **WHEN** Transfer 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: TreeSelect 原生合同
系统 SHALL 为 TreeSelect 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: TreeSelect 正常交互及状态更新
- **WHEN** 应用挂载 TreeSelect 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: TreeSelect 禁用和生命周期
- **WHEN** TreeSelect 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Upload 原生合同
系统 SHALL 为 Upload 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。原生文件选择/拖入、受控文件列表、类型与数量限制、进度/失败/取消/重试、预览及可注入异步传输。

#### Scenario: Upload 正常交互及状态更新
- **WHEN** 应用挂载 Upload 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Upload 禁用和生命周期
- **WHEN** Upload 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 独立页面和真实支持状态
系统 SHALL 让每个有效组件目录项拥有独立说明和真实示例，仅在合同实现并通过相应自动验证后提升支持状态。平台未验证不得伪装为通过，既有通过证据不得因切换机器而回退。

#### Scenario: 完成单个组件阶段
- **WHEN** 一个组件的新增功能及受影响自动检查完成
- **THEN** 页面、支持清单和精简验收同步更新；原始运行产物仅保存在 out，尚未实现的行为仍明确列出。
