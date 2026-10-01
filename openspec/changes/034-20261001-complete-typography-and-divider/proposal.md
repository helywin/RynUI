# Why

Ant Design 的标题层级、正文、段落、链接和分割线属于最基础的排版能力，但 RynUI 目前只有 `ryn::Text` 一个正文组件：没有 `Title`/`Paragraph`/`Link`，没有 `type`、`strong`、`italic`、`underline`、`delete`、`code`、`keyboard`、`mark` 等行内语义，没有 `ellipsis`、`copyable`、`editable`，也没有 `Divider`。Gallery 目录中 `ant.component.typography` 只能声明 partial，`ant.component.divider` 仍是 planned，组件样例只能用 `Text` 与手写线条拼凑，无法验证分页组件与文本场景、输入运行时、剪贴板和焦点模型的协作。

按用户决定，本 change 以「尽量接近 Ant Design」为目标做完整 Typography，并把 Divider 一并纳入，使基础排版成为后续 Card、List、Descriptions、Form 等组件的可复用前置能力。

# What Changes

- 锁定并新增 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 图标资源，沿用 033 的离线资源、许可证、SHA256 与内嵌轮廓容器流程。
- 提供 `ryn::Title`（level 1–5）、`ryn::Text`、`ryn::Paragraph`、`ryn::Link`，使用 typed Props、typed slot 与 reactive `Prop<T>`，并保留现有 `ryn::Text(String)` 便利重载。
- 行内语义：`type`（secondary/success/warning/danger）、`disabled`、`strong`、`italic`、`underline`、`delete`、`code`、`keyboard`、`mark`。文本装饰以 glyph 场景之上的 quad overlay 绘制，颜色、线宽和偏移来自 Typography Component Token；`code`/`keyboard` 使用等宽字族。
- 启用 `SystemFontFamily::ui_monospace`：字体链按字族分别解析，`code`/`keyboard` 使用真实等宽字形，缺字仍回落现有 fallback。
- `ellipsis`：单行截断、多行 `rows` 截断与 `expandable` 展开/收起，使用现有 text engine 的行与溢出测量结果，保持扩展名（ellipsis 后缀）与字素边界安全；宽度变化后重新截断。
- `copyable`：点击复制图标把内容写入系统剪贴板，成功后图标临时变为 `CheckOutlined` 并在超时后复原；支持自定义提示文案。剪贴板端口未绑定或写入失败时不崩溃、不伪造成功。
- `editable`：点击编辑图标把文字原地切换为单行输入，回车或失焦保存、Esc 取消，受 `maxLength` 与 `disabled` 约束；复用现有 `TextEditorStore`、`TextInputSessionHost` 与 `ryn::Input` 编辑路径。
- `ryn::Divider`：`horizontal`/`vertical`、`orientation`（left/right/center）、`orientationMargin`、`dashed`、`plain`、带文字与无文字，几何与颜色全部来自 Divider Component Token。
- 扩展公开 Theme：`TypographyThemeConfig` 与 `DividerThemeConfig` Component Token override，以及等宽字族 token，沿用现有 `Text`/`Input`/`Switch` 的 override 模式。
- 更新 Gallery：Typography 与 Divider 从元数据条目升级为真实样例，支持状态与说明反映本 change 的实际覆盖范围，并补齐 support overlay 与 reference catalog 合同。

# Capabilities

## New Capabilities

- `typography`：标题/正文/段落/链接的公开 API、行内语义、省略、复制与编辑合同。
- `divider`：水平与垂直分割线的公开 API、文字布局与主题合同。

## Modified Capabilities

无主规格变更。仓库当前没有 `openspec/specs/` 主规格（尚未 archive 任何 change），`ant.component.typography` 与 `ant.component.divider` 的支持范围属于 Gallery 离线参考数据，在本 change 中作为实现范围的一部分更新，不构成对既有主规格的修改。

# Impact

影响公开 API（新增 `include/ryn/typography.hpp`、`include/ryn/divider.hpp` 与 `theme.hpp` 的 Component Token 配置）、组件宿主（新增 Typography/Divider host，复用 `WindowComponentServices`、`PressableBehavior`、焦点、动画与 retained surface）、文字与场景链路（等宽字族、装饰 overlay、截断测量）、图标资源与生成工具、以及 Gallery 目录与样例。不新增第三方依赖，不引入 React、CSS-in-JS 或通用视觉 `Modifier`。

主要风险：

- 等宽字族需要扩展默认字体链的按字族解析，平台字体名与缺字回退必须在 Windows 与 Linux 分别验证。
- 文本装饰 quad 与 glyph 的基线对齐、多行段落逐行装饰、以及裁剪与滚动变换必须与文字场景保持同一坐标系，避免错位。
- `ellipsis` 的逐字素收缩与重新测量会触碰 text engine 的测量结果，必须复用现有缓存与局部失效，不能每次更新整段重排无关组件。
- `copyable`/`editable` 会新增剪贴板与输入运行时依赖，必须保持窗口只绑定一次端口、失败可观测、且不因缺少端口而破坏纯文本用法。
- `Link` 与 `editable`/`copyable` 新增可交互元素会改变焦点遍历与命中顺序，必须保持既有 Button/Input/Search 的焦点与命中合同不回归。

本 change 不实现 `Tooltip`（`ellipsis.tooltip` 与 `copyable.tooltip` 只保留 typed 入口与状态契约，真实浮层留给后续 Tooltip change）、`tooltip`/`ellipsis` 的浮层视觉、富文本、`Typography` 的 `setContent` 级联排版、多行文本编辑器（`TextArea`）以及任意值类型的编辑回调。本机验收 Windows MSVC/D3D12 真实窗口；平台通用合同只测一次，不声明 Linux 实机结果。
