# Design

## Context

见 proposal.md；窗口服务已支持 participant、semantic text style、retained surface、PressableBehavior 和自定义 ComponentLayout。本地锁定输入为 out/upstream-ant-design-6.6.5 的 Tag/Badge 源码；Context7 仅作辅助，版本行为以锁定源码为准。

## Goals / Non-Goals

保持一个 retained 标签树和一个 retained 徽标树；不引入 renderer API、VDOM、平台分支或通用视觉样式入口。首版不承担 GPU 人工视觉与系统 IME 验收。

## Decisions

- TagProps 和 CheckableTagProps 分别表达关闭与选择，避免混合两个交互角色。visible/onClose 为完全受控请求；不提供隐式内部关闭状态，用户可直接拒绝请求。
- typed 预设颜色与状态枚举、Color 表达自定义颜色。主题中集中解析默认、状态、预设色与 metrics；颜色订阅和 metrics/文字订阅分开。
- Badge 使用可选整数 count 区分“无计数”和零；dot 无计数显示，显式零遵守 showZero。有效 count 优先于 status；status text 独立于计数文本。
- wrapped Badge 的 child 正常参与测量；indicator 仍是同一树的 retained child，布局后定位并绘制于其上，不扩大包装尺寸。独立模式以 indicator/text 测量。
- 闭包只保留 generation ComponentId，回调前复制事件；销毁重入后不再访问旧状态。
- 使用 windows-msvc-headless、Ninja Multi-Config、MSVC Debug/Release，私有 out 构建与最多四并发；平台通用合同在本机验证一次，不冒充 Linux/Wayland 或人工视觉。

## Risks / Trade-offs

- 指示器超出 child → 使用正常祖先 clip，并测试右上角、偏移及窄约束。
- 关闭或 disable 触发焦点回调重入 → generation lookup、共同 cancel/focus 服务和销毁测试。
- processing 首版为静态状态色；数字直接切换，避免新增动画状态。
