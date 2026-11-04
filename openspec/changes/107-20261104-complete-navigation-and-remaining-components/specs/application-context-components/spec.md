# Spec Delta

## Purpose

规定 RynUI 其他分类可供原生桌面应用依赖的组件行为，沿用锁定 Ant Design 6.6.5 的设计合同，并将功能实现、平台集成与独立 Gallery 示例分别验证，避免以目录占位替代真实交互。

## ADDED Requirements

### Requirement: Affix 原生合同
系统 SHALL 为 Affix 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。显式滚动目标、顶部/底部偏移、位置变化通知及释放恢复。

#### Scenario: Affix 正常交互及状态更新
- **WHEN** 应用挂载 Affix 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Affix 禁用和生命周期
- **WHEN** Affix 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: App 原生合同
系统 SHALL 为 App 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。有作用域的消息/通知/模态服务、上下文继承、窗口隔离及销毁取消。

#### Scenario: App 正常交互及状态更新
- **WHEN** 应用挂载 App 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: App 禁用和生命周期
- **WHEN** App 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: BorderBeam 原生合同
系统 SHALL 为 BorderBeam 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。沿边框运动的主题化效果、时长/方向/颜色和 reduced motion。

#### Scenario: BorderBeam 正常交互及状态更新
- **WHEN** 应用挂载 BorderBeam 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: BorderBeam 禁用和生命周期
- **WHEN** BorderBeam 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: ConfigProvider 原生合同
系统 SHALL 为 ConfigProvider 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。原生 locale/direction/size/disabled/theme 上下文、嵌套覆盖和动态传播。

#### Scenario: ConfigProvider 正常交互及状态更新
- **WHEN** 应用挂载 ConfigProvider 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: ConfigProvider 禁用和生命周期
- **WHEN** ConfigProvider 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: Util 原生合同
系统 SHALL 为 Util 提供 typed Props、响应属性、Theme 视觉和可操作 Gallery。说明上游内部工具与原生公共能力的对应关系；无独立公开 UI 合同的工具不得伪造空组件或虚构 implemented。

#### Scenario: Util 正常交互及状态更新
- **WHEN** 应用挂载 Util 并通过其公开输入或响应属性更新状态
- **THEN** 实际布局、画面和事件符合锁定设计合同；受控请求不擅自覆盖应用状态，未改变的保留内容不重建。

#### Scenario: Util 禁用和生命周期
- **WHEN** Util 被禁用、隐藏、移除，或回调中销毁宿主
- **THEN** 失效内容不再消费输入、捕获或提交异步结果；清理焦点、浮层与动画资源，后续挂载可正常使用。

### Requirement: 独立页面和真实支持状态
系统 SHALL 让每个有效组件目录项拥有独立说明和真实示例，仅在合同实现并通过相应自动验证后提升支持状态。平台未验证不得伪装为通过，既有通过证据不得因切换机器而回退。

#### Scenario: 完成单个组件阶段
- **WHEN** 一个组件的新增功能及受影响自动检查完成
- **THEN** 页面、支持清单和精简验收同步更新；原始运行产物仅保存在 out，尚未实现的行为仍明确列出。
