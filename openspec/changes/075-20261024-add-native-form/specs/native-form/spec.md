# Spec Delta

## Purpose

为 RynUI 提供可组合的原生 typed 表单能力，将现有控件的显式值绑定、字段生命周期、同步校验、反馈和提交重置连接成可用业务流程；在保留输入草稿与组件身份的同时遵守现有焦点、遮罩和主题边界。

## ADDED Requirements

### Requirement: Typed explicit field binding and registration
系统 SHALL 提供 typed Form/FormItem Props、slots 和 reactive Prop，字段支持 String、optional<double>、bool 和现有 SelectSelection；MUST 显式绑定真实控件，不从任意 slot 自动采值。字段名称及 handle 在同一时刻 MUST 唯一注册；卸载后结果中移除该字段且清除错误，handle 保留值。

#### Scenario: Real controls participate in a form
- **WHEN** Input、Select、Checkbox、InputNumber 通过 typed field 连接并编辑
- **THEN** 表单结果包含对应 typed 值，普通值更新不重执行无关 slots。

#### Scenario: Remove and remount a field
- **WHEN** 字段被卸载再用原 handle 注册
- **THEN** 卸载期间结果不含该字段，再挂载保留值且没有旧错误，旧回调不能写入新注册。

### Requirement: Synchronous validation and submission
系统 SHALL 提供 required 与 custom 同步验证、显式 validate 和 submit、成功/失败回调及拥有值的结果快照。验证期间改值、注册变化或销毁 MUST 阻止旧错误提交及旧成功通知；递归命令 MUST 拒绝执行。

#### Scenario: Required failure followed by success
- **WHEN** 提交含无效 required 字段，随后编辑合法值再次提交
- **THEN** 首次给出字段错误及失败通知，第二次成功提交最新值；原控件和 InputNumber 草稿不因校验而重建。

#### Scenario: Reentrant validator or destruction
- **WHEN** validator 改值、移除字段或销毁表单
- **THEN** 不发布旧成功结果，销毁后不调用表单回调且 held FormRef 惰性失效。

### Requirement: Initial values and reset
系统 SHALL 将字段创建初值作为 reset 目标；受控字段 MUST 使用外部 Prop authority 和 callback。程序与原生 Button SHALL 可通过同一 FormRef 提交/重置。

#### Scenario: Reset preserves component identity
- **WHEN** 原生 Button 或程序调用 reset
- **THEN** 非受控值恢复初值、受控值发出初值请求，所有注册字段错误清除，控件不 remount；外部拒绝回写时值仍服从外部 authority。

### Requirement: Interaction boundaries and eligible focus
系统 SHALL 允许将 Form/FormItem disabled/readOnly 显式连接到既有控件能力；被隐藏、Modal 排除或 Spin 阻止的 Form MUST 拒绝 validate/submit，卸载后 ref MUST 无操作。提交失败只可聚焦可交互的无效字段；MUST 保留 TextArea Enter 与 IME 输入语义。

#### Scenario: First eligible invalid field
- **WHEN** 首个无效字段不能交互，而后续无效字段拥有可用真实 ref
- **THEN** 尝试后续字段，焦点不越过祖先隐藏、clip/input block 或 Modal 边界。

#### Scenario: Blocked form and multiline editing
- **WHEN** 表单被 Spin/Modal/hidden 限制或 TextArea/IME 使用 Enter
- **THEN** 前者不提交或抢焦点，后者保持现有原生输入行为，不自动触发表单提交。

### Requirement: Retained feedback and theme
FormItem SHALL 显示标签、required 标记和 help/error，错误优先于 help；空行 MUST 不占布局流且隐藏 glyph。视觉 MUST 由 Theme/Form token 控制，LayoutStyle 只控制外部布局。

#### Scenario: Feedback and theme update
- **WHEN** 校验错误、帮助文本或 Theme 改变
- **THEN** 只更新相应 retained 属性、测量和 glyph，不重执行控件 slot，实际节点/glyph 遵守祖先 clip，既有主题值不变。
