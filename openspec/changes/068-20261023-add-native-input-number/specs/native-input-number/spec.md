# Spec Delta

## Purpose

为桌面业务表单提供允许空值的原生数值输入，在保留真实文本草稿和输入法组合态的同时，提供可靠的范围、精度和鼠标键盘步进，并遵守受控值、焦点生命周期及主题的共同合同。

## ADDED Requirements

### Requirement: Optional numeric state and draft

组件 SHALL 提供 typed reactive Props、可选数值 controlled/default 模式及 typed callbacks，拒绝冲突或不支持的数值且不部分更新。组件 MUST 保留空值及不完整文本草稿，只有完整合法数值或空值触发 change；controlled 拒绝后提交恢复权威值。

#### Scenario: Partial edit and controlled refusal
- **WHEN** 用户依次输入符号、尾点和完整小数，且受控回调拒绝候选值
- **THEN** 不完整草稿保留但不发布数值，完整草稿发布候选，失焦或 Enter 后恢复受控值

#### Scenario: Invalid reactive update
- **WHEN** value、range、step 或 precision 更新为非法配置
- **THEN** 更新被拒绝，既有数值、草稿及交互状态不部分更新

### Requirement: Decimal range and stepping

组件 SHALL 支持 min/max、正 step 和 precision 0..12，十进制步进避免常见二进制浮点累积漂移。blur/Enter SHALL 舍入并限制用户草稿到范围；外部受控越界值和动态范围变更 SHALL 不自动发 change。滚轮 SHALL 默认不改变值。

#### Scenario: Decimal step and bound
- **WHEN** 0.1 以 0.1 连续步进，或在边界、空值、负数和 precision 下步进
- **THEN** 返回规范十进制结果，不越过边界；空值从 0 开始计算；边界无变化时不发 change/step

#### Scenario: Precision cannot reverse a step
- **WHEN** 非空权威值因初值、外部回写或 precision/范围变更而不在精度网格，用户请求 Up 或 Down
- **THEN** 舍入和夹取结果不得反向；有合法结果时沿请求方向变化，否则保持原值且不发 change/step

### Requirement: Retained native input lifecycle

组件 MUST 复用既有单个 Input/editor/session、focus/ref 及 typed slots，支持按钮、Up/Down、disabled/readOnly、三尺寸与四种 Input variants。preedit MUST 不解析或发布，销毁后 session/ref/actions 全部失效；普通属性更新 MUST 保留身份。

#### Scenario: IME and disposal
- **WHEN** 组合输入进行中执行步进，随后 commit 并销毁
- **THEN** preedit 不改变数值，commit 按文本编辑合同处理，销毁清理焦点/session/ref 并拒绝迟到事件

#### Scenario: Selection cancels preedit
- **WHEN** ref selection、focus cursor 或 pointer selection 取消 preedit
- **THEN** 不发布预编辑文本，按钮资格反映最新编辑器状态并保留 disabled/readOnly 和逻辑输入门禁；门禁允许后按钮和键盘步进可用

### Requirement: Theme isolation

视觉 SHALL 由 Input 表面 tokens 和 typed InputNumber handle tokens 驱动，支持继承、Dark/Compact 与独立失效。颜色更新 MUST 不扩大为测量，旧 Theme golden 字段 MUST 保持。

#### Scenario: Handle token update
- **WHEN** 仅改变 handle 颜色或宽度
- **THEN** 分别只请求 material 或所需 layout/geometry，保留 Input 和 action identities，旧组件字段不改变
