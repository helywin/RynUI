# Spec Delta

## Purpose

规定 RynUI 数据展示分类可供原生桌面应用依赖的组件行为，沿用锁定 Ant Design 6.6.5 的设计合同，并将功能实现、平台集成与独立 Gallery 示例分别验证，避免以目录占位替代真实交互。

## ADDED Requirements

### Requirement: Avatar 原生合同
系统 SHALL 为 Avatar 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Avatar 正常交互及状态更新
- **WHEN** 应用挂载 Avatar 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Avatar 禁用和生命周期
- **WHEN** Avatar 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Badge 原生合同
系统 SHALL 为 Badge 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Badge 正常交互及状态更新
- **WHEN** 应用挂载 Badge 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Badge 禁用和生命周期
- **WHEN** Badge 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Calendar 原生合同
系统 SHALL 为 Calendar 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。公历年月视图、受控选择/范围、locale、单元格及 header slots。

#### Scenario: Calendar 正常交互及状态更新
- **WHEN** 应用挂载 Calendar 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Calendar 禁用和生命周期
- **WHEN** Calendar 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Card 原生合同
系统 SHALL 为 Card 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Card 正常交互及状态更新
- **WHEN** 应用挂载 Card 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Card 禁用和生命周期
- **WHEN** Card 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

#### Scenario: Card 显式圆角裁剪保留后代
- **WHEN** Card 的 clipContent 或 loading 状态启用圆角裁剪，或在保留子树上动态改变该状态、尺寸及 Theme radius
- **THEN** 普通后代的 Quad、Glyph、Image、Effect 与子阴影通过共同圆角组合成且命中同一几何交集；自身 outer shadow 保留，窗口层浮层遵循既有逃逸与输入权威；未变化的 slots、编辑会话和焦点身份不重建，普通 Card 默认保留溢出行为。

#### Scenario: Card 嵌套透明边缘
- **WHEN** 半透明内容穿过 Card 圆角，或存在嵌套 rounded masks 和祖先 opacity
- **THEN** 每个不同组只应用一次 mask coverage，self surface 不重复 AA，PM 合成不重复乘 alpha 或祖先 opacity；不同嵌套过渡带遵循有序 alpha-mask 合同，硬裁剪与物理像素 guard 保持独立。

### Requirement: Carousel 原生合同
系统 SHALL 为 Carousel 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留面板、切换/拖动/键盘、方向、指示器、自动播放及暂停、有限动画。

#### Scenario: Carousel 正常交互及状态更新
- **WHEN** 应用挂载 Carousel 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Carousel 禁用和生命周期
- **WHEN** Carousel 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Collapse 原生合同
系统 SHALL 为 Collapse 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Collapse 正常交互及状态更新
- **WHEN** 应用挂载 Collapse 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Collapse 禁用和生命周期
- **WHEN** Collapse 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Descriptions 原生合同
系统 SHALL 为 Descriptions 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Descriptions 正常交互及状态更新
- **WHEN** 应用挂载 Descriptions 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Descriptions 禁用和生命周期
- **WHEN** Descriptions 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Empty 原生合同
系统 SHALL 为 Empty 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Empty 正常交互及状态更新
- **WHEN** 应用挂载 Empty 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Empty 禁用和生命周期
- **WHEN** Empty 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Image 原生合同
系统 SHALL 为 Image 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。解码资源、占位/失败/回退、尺寸适配、预览分组与缩放/旋转/拖动。

#### Scenario: Image 正常交互及状态更新
- **WHEN** 应用挂载 Image 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Image 禁用和生命周期
- **WHEN** Image 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 废弃 List 的兼容说明
系统 SHALL 保持 List 的 deprecated 状态并指向 Listy，不得为填满目录复活已废弃的公开组件。

#### Scenario: 打开旧目录项
- **WHEN** 应用或用户查看 List 页面
- **THEN** 页面明确说明替代组件和实际支持范围。

### Requirement: Listy 原生合同
系统 SHALL 为 Listy 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。typed 稳定键控数据、保留 item slots、方向/网格、分页/滚动及空状态。

#### Scenario: Listy 正常交互及状态更新
- **WHEN** 应用挂载 Listy 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Listy 禁用和生命周期
- **WHEN** Listy 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Popover 原生合同
系统 SHALL 为 Popover 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Popover 正常交互及状态更新
- **WHEN** 应用挂载 Popover 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Popover 禁用和生命周期
- **WHEN** Popover 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: QRCode 原生合同
系统 SHALL 为 QRCode 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。标准 QR 编码、可验证内容、尺寸/色彩/纠错、状态覆盖和图像导出。

#### Scenario: QRCode 正常交互及状态更新
- **WHEN** 应用挂载 QRCode 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: QRCode 禁用和生命周期
- **WHEN** QRCode 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Segmented 原生合同
系统 SHALL 为 Segmented 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Segmented 正常交互及状态更新
- **WHEN** 应用挂载 Segmented 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Segmented 禁用和生命周期
- **WHEN** Segmented 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Statistic 原生合同
系统 SHALL 为 Statistic 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Statistic 正常交互及状态更新
- **WHEN** 应用挂载 Statistic 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Statistic 禁用和生命周期
- **WHEN** Statistic 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Table 原生合同
系统 SHALL 为 Table 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Table 正常交互及状态更新
- **WHEN** 应用挂载 Table 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Table 禁用和生命周期
- **WHEN** Table 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Tag 原生合同
系统 SHALL 为 Tag 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Tag 正常交互及状态更新
- **WHEN** 应用挂载 Tag 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Tag 禁用和生命周期
- **WHEN** Tag 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Timeline 原生合同
系统 SHALL 为 Timeline 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Timeline 正常交互及状态更新
- **WHEN** 应用挂载 Timeline 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Timeline 禁用和生命周期
- **WHEN** Timeline 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Tooltip 原生合同
系统 SHALL 为 Tooltip 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Tooltip 正常交互及状态更新
- **WHEN** 应用挂载 Tooltip 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Tooltip 禁用和生命周期
- **WHEN** Tooltip 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Tour 原生合同
系统 SHALL 为 Tour 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。原生目标定位、遮罩高亮、步骤导航、开合/关闭、焦点与离页清理。

#### Scenario: Tour 正常交互及状态更新
- **WHEN** 应用挂载 Tour 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Tour 禁用和生命周期
- **WHEN** Tour 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Tree 原生合同
系统 SHALL 为 Tree 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。保留已实现的原生合同，并补齐设计矩阵所列原生缺口；与该组件相关的 Web 专用接口和平台实测边界分开说明。

#### Scenario: Tree 正常交互及状态更新
- **WHEN** 应用挂载 Tree 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Tree 禁用和生命周期
- **WHEN** Tree 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 独立页面和真实支持状态
系统 SHALL 让每个有效组件目录项拥有独立说明和真实示例，仅在合同实现并通过相应自动验证后提升支持状态。平台未验证不得伪装为通过，既有通过证据不得因切换机器而回退。

#### Scenario: 完成单个组件阶段
- **WHEN** 一个组件的新增功能及受影响自动检查完成
- **THEN** 页面、支持清单和精简验收同步更新；原始运行产物仅保存在 out，尚未实现的行为仍明确列出。
