# Spec Delta

## Purpose

在 RynUI 离线 Gallery 中提供能够真实操作的业务录入样例，使用原生 Form、DatePicker 和 TimePicker 展示从填写到验证、提交和重置的完整流程。样例应可由目录到达，保持草稿和控件身份，并准确区分组件能力、共同逻辑与原生平台验收。

## ADDED Requirements

### Requirement: 原生表单闭环

Gallery SHALL 提供绑定真实 Input、Select、InputNumber 和 Checkbox 的 Form 样例，通过原生操作展示必填/自定义同步校验、首个可见无效字段聚焦、成功反馈、重置及禁用/只读；不得用静态文字冒充控件状态。

#### Scenario: 从失败到成功再重置
- **WHEN** 用户提交未完成表单、修正字段后提交，再输入未完成数字草稿并重置
- **THEN** 真实字段展示错误并正确聚焦，成功反馈包含实际接受值，重置清除错误且恢复初值和数字显示；控件身份保持

### Requirement: 日期和时间真实选择

Gallery SHALL 使用已验收的原生日期/时间选择器展示真实弹层、键盘/指针选择、清空与限制，并显式适配为 Form 支持的字段类型；格式转换和提交结果 SHALL 保留受控 authority，不声明 Form 新增 typed 日期时间字段支持。

#### Scenario: 选择与组合提交
- **WHEN** 用户经真实月历/时间列选择有效值并提交组合表单，随后清空或重置
- **THEN** 实际输入显示、选择状态和提交结果一致，非法或未提交草稿不进入成功值，重置恢复样例声明的初值

### Requirement: 可导航且保留内容

Form、DatePicker、TimePicker 的目录入口 SHALL 到达实际对应样例，其他组件和既有标识仍保持原行为。状态、主题或宽度更新 SHALL 保留已挂载控件和草稿，实际 glyph、图示、命中与焦点遵守有效有限视口。

#### Scenario: 目录与宽窄主题切换
- **WHEN** 用户点击目录进入样例、编辑草稿，并在 Default/Dark/Compact 与 1200/560 宽视口之间切换
- **THEN** 可以操作实际样例，控件/slot 身份和草稿保持，内容受有效 clip 约束，其他已有目录入口仍定位原目标

### Requirement: 支持范围和验收记录准确

目录、API 文档与完成清单 SHALL 只声明实际实现及运行验证的能力。未完成的依赖或样例不得作为已交付，Recording/启动 smoke 与组件人工视觉、系统输入、Linux 或辅助技术验收 SHALL 分别记录。

#### Scenario: 分阶段交付
- **WHEN** Form 样例先验证通过而时间组件尚未合入
- **THEN** 只完成 Form 阶段，日期/时间阶段保留未完成，最终完整 Gallery 验收在全部样例齐备后单独执行并记录真实 inventory
