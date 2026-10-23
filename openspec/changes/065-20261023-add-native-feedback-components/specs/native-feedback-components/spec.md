# Spec Delta

## Purpose

为 RynUI 原生应用提供可组合的内联状态提示、保留内容的加载反馈和确定进度，使业务能够通过响应式属性更新反馈，并保持原有内容生命周期、主题语义和按需帧调度。

## ADDED Requirements

### Requirement: Controlled native alert

Alert SHALL 支持状态、message、description、图标、action 与 close typed slots，以及受控 visible/closable 和 onClose 请求。

#### Scenario: Close requests write-back
- **WHEN** 用户点击或键盘激活 close
- **THEN** 触发一次 onClose，visible 写回前内容仍显示，隐藏后不接受输入，普通状态和文本更新不重跑 slot

### Requirement: Retained loading shield

Spin SHALL 保留包裹内容，spinning 时显示 dim shield 与 indicator，阻止内容子树的 pointer、focus、keyboard 和编辑输入，同时保留其业务状态。

#### Scenario: Loading toggles
- **WHEN** spinning 从 false 切为 true 再恢复
- **THEN** 内容只挂载一次，已有捕获和焦点被清理，加载时动态子控件同样不可操作，恢复后原控件继续可用

### Requirement: On-demand finite spinner

Spin SHALL 只在可见、窗口活动且 motion 允许时调度有限动画周期，reduced 或关闭 motion 时提供静态加载提示。

#### Scenario: Animation becomes inactive
- **WHEN** spinner 离屏、祖先隐藏、窗口失活、关闭加载或销毁
- **THEN** 停止动画 deadline，重新可见且活动时恢复；静态提示仍可表达加载状态

### Requirement: Determinate native progress

Progress SHALL 提供 line/circle/dashboard、响应式 percent/status/size/showInfo，有限 percent 截断到 0..100，非有限值和非法枚举拒绝。

#### Scenario: Reactive progress
- **WHEN** percent 从 25 更新为 100 或 status 更新为 Exception
- **THEN** 原地更新进度形状、文字和状态色，100 默认完成态，Exception 保持错误态，不重建无关内容

### Requirement: Feedback themes and retained scene

反馈组件 SHALL 使用 typed Theme token 与外部 LayoutStyle，继承 Default/Dark/Compact 和 nested overrides，不改变 CPU scene v3/GPU ABI v2。

#### Scenario: Material-only theme change
- **WHEN** 只修改反馈组件颜色 token
- **THEN** 不重新测量组件，不重跑 slot，旧组件序列化字段除 identity 外保持不变
