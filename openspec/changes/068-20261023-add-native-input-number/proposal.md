# Proposal

## Why

业务表单需要能保留空值、未完成输入并可靠步进的原生数值控件。现有 Input 已有 retained 编辑、IME session、焦点和 ref，应在其上增加数值语义。

## What Changes

- 新增 typed `InputNumberProps`、可选 double 值、范围/步长/precision、typed change/step/submit callbacks、prefix/suffix slots。
- 复用真实 Input/editor/session，支持按钮和 Up/Down、三种尺寸、现有四种 Input variant、disabled/readOnly、ref。
- 保留编辑草稿，preedit 不解析；blur/Enter 规范化，controlled 拒绝后回到权威值；十进制定点步进避免常见浮点漂移。
- 首版限定安全整数范围和最多 12 位小数；stringMode、任意精度、formatter/parser 服务、长按连发、wheel stepping、spinner 模式后续独立设计。

## Capabilities

### New Capabilities

- `native-input-number`：原生可选数值编辑、范围和步进、retained 输入与主题行为。

### Modified Capabilities

无。

## Impact

新增公开头、局部数值模型与 Input 集成、InputNumber typed theme tokens、CMake 测试入口及专项文档。不修改 scene v3/GPU ABI v2，不引入依赖。仅当前隔离工作树本地提交；Gallery、README、支持矩阵和父级集成由父任务负责。
