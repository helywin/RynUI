## Purpose

普通原生应用使用系统字体时应能显示示例中的常见Unicode内容，同时保留真实字形选择、塑形和光栅行为，避免把字体缺失隐藏为文本替换或额外部署字体依赖。

## ADDED Requirements

### Requirement: Usable system fallback
Windows系统字体链 SHALL 为示例中的常见emoji选择可塑形且可光栅的字形，并保留中英与组合字符内容。
#### Scenario: Input grapheme examples
- **WHEN** 普通Gallery显示中文 / RynUI 🙂及组合重音/emoji样例
- **THEN** 支持的字符使用真实字形，不被替换为U+FFFD，字素统计和编辑语义保持

