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

系统 SHALL 保留现有解析 arc/默认 round/solid 合同，并通过共同 immutable logical paint、typed Round/Butt/Square cap 和显式 Effect Paint ABI v1 完成连续 linear/conic 渐变。基础 scene ABI v2、112-byte Effect 与 Quad/Glyph/Image 保持。native 使用既有 Color 数值域的 premultiplied 插值和单 fragment 双场 source-over；不声明未锁定浏览器的像素一致性。

#### Scenario: 不同类型的渐变坐标与步骤组合
- **WHEN** 应用改变 percent、RTL、gap placement、stops 或 steps
- **THEN** Line 的 field 跟随当前 percent track box；Circle 的 conic/linear 双场保持完整 circle box，gap placement 仅旋转 mask，按固定 rc 1.0.3 的 floor/顺序 fixup 映射 stop；steps 仅改变 coverage gap，不重启 field 或回退纯色。

#### Scenario: 精确端帽与透明度
- **WHEN** 应用使用新显式 cap、任意合法 gap、厚描边、近整圈或半透明 stops
- **THEN** cap 的解析联合外边界只计算一次 coverage；内部重叠不重复 blend，两场 source-over 后统一应用 opacity；旧默认 round 调用不被强行套用新的组件 percent 补偿。

#### Scenario: 不支持能力、非法输入与共同上传失败
- **WHEN** live scene 要求 backend 不支持的新 paint/cap、资源超限、非法 stop/domain，或共同资源上传/commit 失败
- **THEN** 语义/资源 preflight 在 begin 前拒绝不支持或超限输入；任何事务失败使附件失效并保留原子重试数据，不静默丢弃渐变或提交旧画面；owner/epoch/retire/abandon 与 source ownership 保持 renderer 合同。

#### Scenario: 静态渐变与 active 生命周期
- **WHEN** 静态渐变闲置，或 active Progress 被隐藏、离页、窗口失活或进入 reduced motion
- **THEN** 静态 field 不申请后续帧，active 停止 deadline 并保留正确的静态 field/percent；普通属性更新保留无关组件与富内容身份。

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
