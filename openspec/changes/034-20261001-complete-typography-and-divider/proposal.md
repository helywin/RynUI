# Why

Ant Design 的标题层级、正文、段落、链接和分割线属于最基础的排版能力，但 RynUI 目前只有 `ryn::Text` 一个正文组件：没有 `Title`/`Paragraph`/`Link`，没有 `type`、`strong`、`italic`、`underline`、`delete`、`code`、`keyboard`、`mark` 等行内语义，没有 `ellipsis`、`copyable`、`editable`，也没有 `Divider`。Gallery 目录中 `ant.component.typography` 只能声明 partial，`ant.component.divider` 仍是 planned，组件样例只能用 `Text` 与手写线条拼凑，无法验证分页组件与文本场景、输入运行时、剪贴板和焦点模型的协作。

按用户决定，本 change 以「尽量接近 Ant Design」为目标做完整 Typography，并把 Divider 一并纳入，使基础排版成为后续 Card、List、Descriptions、Form 等组件的可复用前置能力。

# What Changes

- 锁定并新增 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 图标资源，沿用 033 的离线资源、许可证、SHA256 与内嵌轮廓容器流程。
- 提供 `ryn::Title`（level 1–5）、`ryn::Text`、`ryn::Paragraph`、`ryn::Link`，使用 typed Props、typed slot 与 reactive `Prop<T>`，并保留现有 `ryn::Text(String)` 便利重载。
- 行内语义：`type`（secondary/success/warning/danger）、`disabled`、`strong`、`italic`、`underline`、`delete`、`code`、`keyboard`、`mark`。背景与边框（`mark`、`code`、`keyboard`）绘制在字形之前，下划线与删除线绘制在字形之后，位置与厚度取自字体装饰度量；颜色与度量来自 Typography Component Token；`code`/`keyboard` 使用等宽字族。
- 启用 `SystemFontFamily::ui_monospace`：字体链按字族、字重、斜体与像素尺寸分别解析并缓存，`code`/`keyboard` 使用真实等宽字形，`strong`/`italic` 使用真实 face，缺字仍按回退链逐字素混排。
- `ellipsis`：单行以自然宽度与可用宽度比较判定截断，多行按测量行数与 `rows` 比较并在末行留足后缀与操作入口宽度，`expandable` 提供展开/收起；按字素边界收缩，宽度变化后重新截断。
- `copyable`：点击复制图标把**原始全文**写入系统剪贴板，成功后图标临时变为 `CheckOutlined` 并在超时后复原；支持自定义提示文案。剪贴板绑定独立于文本输入，端口未绑定或写入失败时不崩溃、不伪造成功。
- `editable`：编辑入口在首次挂载时预建的单行输入子树间切换，回车或失焦保存、Esc 取消（先取消输入法组合、再次取消才放弃草稿），受 `maxLength` 与 `disabled` 约束；编辑态继承被编辑元素的字族、字重、字号与行高。
- `ryn::Divider`：`horizontal`/`vertical`、`orientation`（left/right/center）、`orientationMargin`、`dashed`、`plain`、带文字与无文字，几何与颜色全部来自 Divider Component Token；不提供交互状态或 `disabled`。
- 扩展公开 Theme：`TypographyThemeConfig` 与 `DividerThemeConfig` Component Token override，以及等宽字族 token，沿用现有 `Text`/`Input`/`Switch` 的 override 模式。
- 更新 Gallery：Typography 与 Divider 从元数据条目升级为真实样例，支持状态与说明反映本 change 的实际覆盖范围，并补齐 support overlay 与 reference catalog 合同。

# Capabilities

## New Capabilities

- `typography`：标题/正文/段落/链接的公开 API、行内语义、省略、复制与编辑合同。
- `divider`：水平与垂直分割线的公开 API、文字布局与主题合同。

## Modified Capabilities

无主规格变更。仓库当前没有 `openspec/specs/` 主规格（尚未 archive 任何 change），`ant.component.typography` 与 `ant.component.divider` 的支持范围属于 Gallery 离线参考数据，在本 change 中作为实现范围的一部分更新，不构成对既有主规格的修改。

# Impact

影响公开 API（新增 `include/ryn/typography.hpp`、`include/ryn/divider.hpp` 与 `theme.hpp` 的 Component Token 配置）、组件宿主（新增 Typography/Divider host，复用 `WindowComponentServices`、`PressableBehavior`、焦点、动画与 retained surface）、文字与场景链路（等宽字族、字重/斜体 face 解析、字体装饰度量、装饰分层、截断测量）、窗口服务（独立于文本输入的剪贴板绑定）、输入运行时（编辑子树预建、字体继承入口、编辑生命周期回调与延迟焦点事务）、图标资源与生成工具、以及 Gallery 目录与样例。不新增第三方依赖，不引入 React、CSS-in-JS 或通用视觉 `Modifier`。

规划评审确认了四项**前置工作**，它们属于本 change 的实现范围并排在对应功能之前：

- `font::FontMetrics` 目前没有 underline/strikeout 的位置与厚度，装饰线无法准确定位，需要扩展字体度量并标注 display scale。
- 默认字体链忽略 weight、Windows 初选固定 NORMAL weight/style，且 `SemanticTypography` 没有 slant；`strong`/`italic` 需要真实的 face 解析与缓存键扩展，仅改数值不会改变字形。
- `bind_text_edit` 目前只由 `InputComponentHost` 触发，纯 Typography 窗口拿不到剪贴板，需要独立于文本输入的剪贴板绑定与晚绑定通知。
- 点击后动态挂载子树的路径在当前运行时不存在（`active_*_host` 只在 mount 期间有效，`ComponentHost` 禁止二次 mount），`editable` 必须改为首次挂载预建编辑子树，并补充字体继承入口、blur/cancel 回调与「派发之后执行」的焦点事务。

若需要收窄范围，正确做法是拆分后续 change，而不是把这些前置工作降级为近似实现。

主要风险：

- 上述前置工作扩大了实现面，且会改动内部 `ThemeFontResolver` 签名与 Input 内部入口，需同步更新既有测试夹具。
- 等宽字族与字重/斜体 face 解析依赖平台字体能力，Windows DirectWrite 与 Linux Fontconfig 的匹配结果必须在各自平台分别验证。
- 装饰必须分两层：背景（`mark`、`code`、`keyboard`）在字形之前，下划线与删除线在字形之后，否则会覆盖或染色文字。
- `ellipsis` 的单行截断不能依赖换行后的 overflow 标志，且候选测量不得污染 retained scene，需要明确的塑形/测量次数合同。
- `copyable`/`editable` 会新增剪贴板与输入运行时依赖，必须保持窗口只绑定一次端口、失败可观测、且不因缺少端口而破坏纯文本用法。
- `Link` 与 `editable`/`copyable` 新增可交互元素会改变焦点遍历与命中顺序，必须保持既有 Button/Input/Search 的焦点与命中合同不回归。

本 change 不实现 `Tooltip`（`ellipsis.tooltip` 与 `copyable.tooltip` 只保留 typed 入口与状态契约，真实浮层留给后续 Tooltip change）、富文本、`Typography` 的 `setContent` 级联排版、多行文本编辑器（`TextArea`）以及任意值类型的编辑回调。`Divider` 不提供交互状态或 `disabled`。本机验收 Windows MSVC/D3D12 真实窗口；平台通用合同只测一次，不声明 Linux 实机结果。
