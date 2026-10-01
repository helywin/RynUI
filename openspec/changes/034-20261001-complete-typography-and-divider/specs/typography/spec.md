## ADDED Requirements

### Requirement: Typography public API

系统 SHALL 提供 `ryn::Title`（level 1–5）、`ryn::Text`、`ryn::Paragraph` 与 `ryn::Link`，全部使用 typed Props、typed slot 与 reactive `Prop<T>`，`LayoutStyle` 只控制外部布局。现有 `ryn::Text(String)` 便利重载与 `TextProps` 行为 MUST 保持源码兼容。

#### Scenario: Declare each typography form
- **WHEN** 应用在同一页面声明标题、正文、段落与链接
- **THEN** 每个形式使用各自的 typed builder，内容可来自静态字面量或 `Prop<String>`，不需要应用直接操作文本场景或图形 primitive

#### Scenario: Title levels
- **WHEN** 应用声明 level 1 到 5 的标题
- **THEN** 每个级别使用 Ant Design 6.6.5 的对应字号、行高与标题间距，级别可在挂载后变化并保持组件 identity

#### Scenario: Reactive content and semantics
- **WHEN** 内容、`type`、`disabled` 或任一语义属性在挂载后变化
- **THEN** 只更新受影响的文本、材质或几何范围，不重新执行无关父组件

### Requirement: Inline typography semantics

Typography SHALL 支持 `type`（secondary/success/warning/danger）、`disabled`、`strong`、`italic`、`underline`、`delete`、`code`、`keyboard` 与 `mark`，其颜色、字重、字体族与装饰几何 SHALL 来自 Theme 与 Typography Component Token。

#### Scenario: Semantic text colors
- **WHEN** 文字使用 secondary、success、warning、danger 或 disabled
- **THEN** 前景色分别取对应语义 Token，在亮色、暗色与紧凑主题下都可辨认，文字内容与布局尺寸语义不变

#### Scenario: Emphasis and decoration
- **WHEN** 文字使用 strong、italic、underline、delete 或 mark
- **THEN** strong 使用强调字重，italic 使用斜体字族选择，underline 与 delete 在正确的基线位置绘制装饰线，mark 使用背景高亮；装饰线与高亮与字形保持同一坐标系，在滚动与裁剪下不错位

#### Scenario: Monospace semantics
- **WHEN** 文字使用 `code` 或 `keyboard`
- **THEN** 使用等宽字族渲染，`code` 带底色与内边距，`keyboard` 带边框与键帽外观；等宽字族缺少字形时仍按既有回退链渲染，不显示缺字方框

#### Scenario: Decoration-only updates stay local
- **WHEN** 只改变装饰或高亮颜色
- **THEN** 不触发重新塑形或重新布局，只更新对应材质与装饰几何

### Requirement: Typography theme contract

标题各级字号与行高、标题间距、正文颜色、等宽字族与装饰颜色 SHALL 来自 Theme 与 Typography Component Token；等宽字族 SHALL 通过 `SystemFontFamily::ui_monospace` 由字体链按字族解析。

#### Scenario: Theme algorithm and overrides
- **WHEN** 当前 Theme 使用 Default、Dark、Compact 算法或自定义 `TypographyThemeConfig`
- **THEN** 标题字号/行高/间距、语义颜色与等宽字族按解析后的快照生效，等值更新不请求帧

#### Scenario: Per-family font resolution
- **WHEN** 同一页面同时渲染 UI 字族与等宽字族文本
- **THEN** 字体链按请求的字族分别解析并缓存，两族文本互不改变对方的字形或度量；DPI 变化后按新像素尺寸重新解析

#### Scenario: Typography color update stays local
- **WHEN** 主题只改变 Typography 的颜色
- **THEN** 不重新测量或重新布局，也不重建无关组件的 scene fragment

### Requirement: Typography ellipsis

`ellipsis` SHALL 支持单行截断、多行 `rows` 截断与 `expandable` 展开/收起；截断 SHALL 按字素边界执行，SHALL 在宽度或内容变化后重新计算，并 SHALL 提供可观察的截断状态。

#### Scenario: Single-line truncation
- **WHEN** 文本在可用宽度内放不下且启用单行 `ellipsis`
- **THEN** 文本截断到能容纳省略后缀的最长字素边界并显示省略后缀，不出现半个字素、不溢出容器、不裁掉相邻组件

#### Scenario: Multi-line rows
- **WHEN** 文本超过 `rows` 指定的行数
- **THEN** 只显示前 `rows` 行并在最后一行按同一规则加省略后缀，行高与基线保持一致

#### Scenario: Expand and collapse
- **WHEN** `ellipsis.expandable` 生效且用户激活展开入口
- **THEN** 显示完整文本并提供可再次收起的入口，展开与收起保持组件 identity、焦点顺序与滚动位置语义

#### Scenario: Re-truncate after resize
- **WHEN** 容器宽度变宽或变窄，或文本内容变化
- **THEN** 使用新宽度重新计算截断结果，结果单调且可重复，不因连续变化累积错误长度

### Requirement: Typography copyable and editable

`copyable` SHALL 在文本旁提供复制入口并把内容写入系统剪贴板；`editable` SHALL 支持把文本原地切换为单行编辑。两者 SHALL 复用窗口已有的剪贴板端口与文本编辑运行时，SHALL 支持响应式开关、`disabled` 与自定义提示文案，且 SHALL NOT 在端口缺失或平台失败时伪造成功。

#### Scenario: Copy text
- **WHEN** 用户激活可用的复制入口
- **THEN** 当前文本写入系统剪贴板，入口显示成功反馈并按配置时长复原；剪贴板端口未绑定或写入失败时保持原状并可观察失败

#### Scenario: Edit text in place
- **WHEN** 用户激活可用的编辑入口并修改文本后按回车或移出焦点
- **THEN** 文本原地进入单行编辑、提交后显示新内容并回调新值；Esc 取消后恢复原内容，受 `maxLength` 与 `disabled` 约束

#### Scenario: Interaction does not disturb neighbours
- **WHEN** 复制或编辑入口获得焦点、被键盘激活或组件被销毁
- **THEN** Tab 顺序、焦点呈现与命中顺序保持确定，销毁后不残留指针捕获、编辑会话或剪贴板反馈定时器

#### Scenario: Keep plain text usable without platform ports
- **WHEN** 窗口没有绑定剪贴板或文本输入端口
- **THEN** 不启用 `copyable`/`editable` 的普通文字仍完整渲染与排版，组件不抛出、不请求额外帧

### Requirement: Link interaction contract

`ryn::Link` SHALL 使用链接语义颜色，并 SHALL 接入与 Button 一致的指针、键盘激活与焦点模型。

#### Scenario: Activate a link
- **WHEN** 用户用指针点击、或用 Tab 聚焦后按激活键
- **THEN** 触发 `onClick` 一次，悬浮、按下与键盘焦点呈现来自主题且可辨认

#### Scenario: Disabled link
- **WHEN** 链接处于 `disabled`
- **THEN** 不参与命中、不获得焦点、不触发回调，呈现使用禁用语义颜色

### Requirement: Typography integration in the reference gallery

Gallery SHALL 为 `ant.component.typography` 提供标题级别、行内语义、省略、复制与编辑的真实样例，并同步 support overlay 的 `supported_scope`、`missing_scope` 与证据标识。

#### Scenario: Browse the typography entry
- **WHEN** 用户在 Token Gallery 打开 Typography 条目
- **THEN** 可以看到五级标题、行内语义、省略与复制/编辑的真实 RynUI 样例，说明反映本 change 的实际覆盖范围与未覆盖范围

#### Scenario: Explicit uncovered scope
- **WHEN** 用户查看 Typography 支持边界
- **THEN** `missing_scope` 明确列出尚未实现的上游能力（例如 `tooltip` 浮层），不把 partial 表述为完整对齐
