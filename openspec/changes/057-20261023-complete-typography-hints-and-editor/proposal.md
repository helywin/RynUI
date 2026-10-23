# Proposal

## Why

Typography 已声明复制、编辑与省略提示文案，但交互宿主未呈现这些提示；编辑态仍使用单行 Input。现有 Tooltip 与 TextArea 已完成原生实现，可以补齐收尾清单中的这两项实际功能缺口。

## What Changes

- 将复制/复制成功、编辑提示和仅在截断时出现的省略提示接入现有 Tooltip，支持响应式文案、空文案关闭、hover 与键盘焦点。
- 以预建 TextArea 替换编辑子树中的 Input，增加 typed autoSize 配置，保留字体继承、IME、受控草稿、提交/取消及生命周期合同。
- 补齐 Gallery、用户文档与实际平台验证记录；Windows 与 Linux 原生验收独立记录。
- 不移植 DOM/CSS 配置，不重写浮层、文本编辑器或上传路径，不将历史人工/原生 Linux 未验收项标记完成。

## Capabilities

### New Capabilities

- `typography-interactions`: Typography 操作提示、截断提示与保留多行编辑的原生交互合同。

### Modified Capabilities

无。当前主规格目录没有已发布 capability；本 change 继承 034 的既有交互行为，不改写历史证据。

## Impact

涉及公开 `TypographyEditable`、Typography/Input 交互宿主、对应测试、Gallery 与文档。复用现有依赖和逻辑场景，不增加第三方依赖。主要风险为 Tooltip 包装后的测量/命中、编辑键盘优先级和隐藏分支清理；通过保留 identity、局部失效、真实编辑与销毁测试验证。
