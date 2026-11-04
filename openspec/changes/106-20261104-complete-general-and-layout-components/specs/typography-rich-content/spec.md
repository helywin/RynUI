## Purpose

为标题和段落提供类型化的混合语义内容，在现有原生文本排版、主题和省略能力上保持完整来源文本，并让桌面剪贴板接收可互操作的纯文本与 HTML 表示。

## ADDED Requirements

### Requirement: 混合语义文本

Typography SHALL 接受不可变 typed runs，支持语义颜色、strong、italic、code、keyboard、mark、underline、strikethrough。换行、省略和双向次序 SHALL 依据完整来源文本；更新样式不重挂载无关组件。

#### Scenario: 中英混排更新
- **WHEN** 混合样式段落发生宽度、主题或内容变化
- **THEN** 使用实际字体与样式重排，字形与装饰对齐，显示省略不修改完整复制内容。

### Requirement: 富文本剪贴板

复制 SHALL 支持纯文本及 HTML 模式；HTML SHALL 转义来源内容并保留语义样式，同时发布纯文本回退。平台失败 SHALL 反馈失败，Core 接口不得依赖 SDL。

#### Scenario: 特殊字符与平台错误
- **WHEN** 复制包含小于号、引号、换行的富文本或平台拒绝写入
- **THEN** HTML 不注入原始标记，纯文本保持原文，错误不显示成功反馈。
