# Design

## Context

RynUI 目前只有 `ryn::Text`（content + tone + LayoutStyle），标题层级、行内语义、省略、复制、编辑和 Divider 都不存在。可复用的地基已经就位：`TextSceneService` 提供 shape/measure/synchronize 与局部失效，`GlyphScene` 的 quad/glyph 命令进入 `ComponentSceneComposer`，`WindowComponentServices` 统一持有窗口级交互、焦点、动画与 retained surface，`PressableBehavior` 和 `TextEditorStore`/`TextInputSessionHost` 已分别被 Button 与 Input 验证，`TextClipboard::write_text` 是平台无关剪贴板端口，033 已建立离线图标资源的锁定与验证流程。

上游基线锁定在 `design-tokens/ant-design/6.6.5`。本 change 涉及的 Component Token 默认值、几何规则与装饰数值取自该 commit 的 `components/typography/style/index.ts`、`components/typography/style/mixins.ts`、`components/divider/style/index.ts` 与 `components/theme/interface/maps/font.ts`：

- Typography Component Token：`titleMarginTop: '1.2em'`、`titleMarginBottom: '0.5em'`。
- 标题字号/行高：h1 38/1.4、h2 30/1.35、h3 24/1.3、h4 20/1.25、h5 16/1.2；标题字重取 `fontWeightStrong`，颜色取 `colorTextHeading`。
- 语义颜色：secondary → `colorTextDescription`，success → `colorSuccessText`，warning → `colorWarningText`，danger → `colorErrorText`（hover/active 用 `colorErrorTextHover`/`colorErrorTextActive`），disabled → `colorTextDisabled`。
- 行内：`strong` 用 `fontWeightStrong`；`code` 用 `fontFamilyCode`、字号 85%、内边距 `0.4em`/`0.2em 0.1em`、半透明底与 1px 半透明边框、圆角 3；`kbd` 用 `fontFamilyCode`、字号 90%、内边距 `0.4em`/`0.15em 0.1em`、下边框 2px；`mark` 用固定黄色高亮；`u`/`ins` 下划线；`s`/`del` 删除线。
- Divider Component Token：`textPaddingInline: '1em'`、`orientationMargin: 0.05`、`verticalMarginInline: marginXS`；水平无线条 `marginBlock: marginLG`，水平带文字 `marginBlock: margin`；带文字使用 `colorTextHeading`、字重 500、`fontSizeLG`；`plain` 回落到 `colorText`、常规字重、`fontSize`；线条颜色 `colorSplit`、线宽 `seed.lineWidth`；垂直分割线 `height: 0.9em`、`top: -0.06em`、`verticalAlign: middle`，`orientationMargin` 未显式设置且 orientation 为 left/right 时轨道宽度归零并使用 `sizePaddingEdgeHorizontal`。

## Goals / Non-Goals

**Goals:** 建立与 Ant Design 6.6.5 一致的公开排版与分割线能力；装饰、省略、复制、编辑全部复用既有文本、交互与剪贴板路径；主题变化只产生最小失效；交互元素遵守既有焦点、命中与销毁合同。

**Non-Goals:** `Tooltip` 浮层本体（`ellipsis.tooltip` 与 `copyable.tooltip` 只发布 typed 入口与状态，真实浮层留给后续 Tooltip change）；`Typography` 的 `setContent` 级联排版与 `ul`/`ol`/`pre`/`blockquote`/`table` reset；富文本；`TextArea` 多行编辑；任意值类型的编辑回调；`copyable.format` 在剪贴板中同时写入 HTML；`dotted` 等本 change 未列出的额外线型。

## Decisions

1. **两个独立组件宿主，各自复用窗口服务。** 新增 `TypographyComponentHost` 与 `DividerComponentHost`，都以 `WindowComponentServices&` 构造并实现 `WindowComponentParticipant`，沿用 `PressableBehavior`、`InteractionRegistry`、`FocusManager`、`AnimationRuntime` 与 retained surface。不把两者塞进 Button 宿主，也不建立新的窗口级单例：宿主数量与 011/019 的控件宿主保持一致。备选是继续扩张 `ButtonComponentHost` 的兼容宿主，会让排版组件依赖按钮语义，且 `friend` 与注册路径继续膨胀。

2. **行内语义全部是 Typography 的属性，不是子组件。** `strong`/`italic`/`underline`/`delete`/`code`/`keyboard`/`mark` 通过 typed Props 表达，因为它们的可见效果（字重、字族、装饰、底色）本来就作用在同一段被塑形的文本上；若拆成嵌套组件就必须实现通用 inline style 合并与文本 run 切分，明显超出本 change。装饰不做字形合成：下划线、删除线、`mark` 高亮、`code`/`keyboard` 的底与边框、keyboard 的下边框全部渲染为 glyph 命令之后的 quad overlay，与字形同一坐标系。

3. **文本装饰几何取测量结果，不取估计值。** `underline`/`delete` 的位置来自 `TextMeasurement` 的逐行 `baseline`/`width` 与该行所用 run 的 `font::FontMetrics` 的装饰字段；多行段落逐行绘制。装饰颜色取文本当前前景，线宽取 `seed.lineWidth`（`keyboard` 下边框为 2 倍）。宽度、字号或文本变化后装饰与文字在同一轮同步里一起重算，避免错位。

4. **等宽字族走 `SystemFontFamily` 参数化字体链。** `make_default_ui_font_resolver` 现在忽略 `SystemFontFamily` 参数（形参未命名），只缓存像素尺寸。改为按字族分别持有字体链并按 `(font_family, pixel_size)` 缓存：`ui_sans` 沿用现有首选/系统回退/CJK 回退，`ui_monospace` 增加各平台等宽首选族（Windows Consolas/Cascadia Mono、Linux DejaVu Sans Mono/Noto Sans Mono）并复用同一缺字回退与验证字体兜底。`ThemeScope::text_font_family()` 已存在，Typography 的 `code`/`keyboard` 用它把 `seed.font_family_code` 传给 `SemanticTypography.font_family`。备选是把 `code` 当普通字族只加底色，会与上游语义不符，且 `font_family_code` 会继续是死 token。

5. **省略按字素边界收缩并复用测量缓存。** `ellipsis` 使用 `TextSceneService` 的测量结果：先按当前宽度测量，若溢出则对能容纳「候选文本 + 省略后缀」的最长字素边界做二分搜索，再以最终文本发布场景。`rows` 用逐行测量裁剪到前 N 行；`expandable` 增加展开/收起状态并切换发布文本。宽度或内容变化后重新计算；同一轮内多次查询复用已同步的度量，不重复塑形。备选是引入文本引擎级截断原语，会把排版策略固化进 engine；本 change 先在组件层实现，engine 只提供测量。

6. **复制复用剪贴板端口，缺端口不伪造成功。** Copy 动作直接调用 `TextClipboard::write_text(StringView)`。`WindowComponentServices` 增加可空剪贴板访问器，并在 `bind_text_edit` 完成时通知已挂载的 `WindowComponentParticipant`（新增带默认实现的 `on_clipboard_available()` 钩子），使「只有 Input 存在时才绑定端口」的现状保持兼容。复制成功后入口临时切换为 `CheckOutlined` 并置为成功色（`map.color_success`），到点复原并取消 deadline；窗口失焦、组件销毁或主题切换时清理。端口未绑定或平台返回失败时保持原状并在诊断中计数，不假装成功。

7. **编辑复用 Input 的编辑运行时与样式继承。** 编辑态在 Typography 宿主内部构建一个真实 `ryn::Input` 并挂载到自己的 typed slot，传入编辑用的初始值、`maxLength` 与提交/取消回调；Input 的窗口服务由同一 `WindowComponentServices` 提供，因此不会二次绑定端口、也不会产生第二个 `TextEditorStore`。编辑态覆盖原文本、回车或失焦提交、Esc 取消。若窗口未绑定文本输入端口，编辑入口不激活并可观察。备选是手写一个 `TextEditor` 内联编辑控件，会重复 Input 已经验收的光标、选区、IME 与剪贴板逻辑。

8. **Divider 是纯几何与材质组件，没有交互状态。** 线、文字与间距全部由 Divider Component Token 派生；`horizontal+with_text` 的轨道按 `orientationMargin` 比例分配宽度（left/right 且未显式设置该值时轨道归零、文字改用 `sizePaddingEdgeHorizontal` 边距），垂直分割线延续行高并保留 `top` 偏移与行内间距。颜色变化只写材质，不触发测量。

9. **主题按既有 Component Token 模式扩展。** `ThemeMapToken` 增加 `color_success_text`/`color_warning_text`/`color_error_text`，`ThemeAliasToken` 增加 `color_split`；新增 `TypographyThemeToken` 与 `DividerThemeToken`、对应 `ThemeConfig` override、`theme_runtime::TokenIdentity` 条目、`dirty_phase_for` 映射与 changed-identity 比较，使「只改颜色不触发测量」由现有失效路径保证，而不是靠组件自行判断。`@ant-design/colors` 派生沿用现有 `palette` 与 `palette_variant` 机制。

10. **图标资源沿用 033 的锁定流程。** 新增 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 五张官方 SVG 到 `third_party/ant-design-icons`（`@ant-design/icons-svg` 4.6.0，锁定 commit 与许可证），扩展 `tools/generate_icon_assets.py` 的图标表并重新生成 `src/icons/ant_design_icon_font.inc` 与 manifest；`tools/verify_icon_assets.py` 的固定数量从 9 改为 14。生成与验证规则不变。

11. **Gallery 与支持范围同步。** `ant.component.typography` 与 `ant.component.divider` 增加真实样例，support overlay 的 `status`、`supported_scope`、`missing_scope`、`evidence_identifiers` 与 reference catalog 合同同步更新，`missing_scope` 明确保留 `tooltip` 浮层等未覆盖项。

## Token 引用清单

实现阶段 MUST 只引用下表 identity；除 `seed.lineWidth` 外都已在 `design-tokens/ant-design/6.6.5/catalog.yaml` 中锁定。`colorSuccessText`/`colorWarningText`/`colorErrorText`（含 hover/active）与 `colorSplit` 目前是 catalog 中的 `metadata` 级 Token，需要在 RynUI Theme 中补出运行时值（见决策 9）。

| 用途 | Token identity | 默认/派生 |
| --- | --- | --- |
| 标题字号 h1–h5 | `ant.map.fontSizeHeading1`–`5` | 38 / 30 / 24 / 20 / 16 |
| 标题行高 h1–h5 | `ant.map.lineHeightHeading1`–`5` | 1.4 / 1.35 / 1.3 / 1.25 / 1.2 |
| 标题字重与颜色 | `ant.alias.fontWeightStrong`、`ant.alias.colorTextHeading` | 600、`colorText` 同源 |
| 标题上下间距 | `ant.component.Typography.titleMarginTop`、`ant.component.Typography.titleMarginBottom` | `1.2em`、`0.5em` |
| 正文与次要文字 | `ant.map.colorText`、`ant.alias.colorTextDescription` | 主文字、次要文字 |
| 语义文字 | `ant.map.colorSuccessText`、`ant.map.colorWarningText`、`ant.map.colorErrorText`、`ant.map.colorErrorTextHover`、`ant.map.colorErrorTextActive` | 成功/警告/错误语义色 |
| 禁用与链接 | `ant.alias.colorTextDisabled`、`ant.map.colorLink` | 禁用文字、链接色 |
| 分割线 | `ant.alias.colorSplit` | 分割线色，线宽取 `seed.lineWidth` |
| 间距 | `ant.alias.marginLG`、`ant.alias.margin`、`ant.alias.marginXS` | 24 / 16 / 8（`sizeUnit` 4 派生） |
| Divider 文字间距与朝向 | `ant.component.Divider.textPaddingInline`、`ant.component.Divider.orientationMargin`、`ant.component.Divider.verticalMarginInline` | `1em`、0.05、`marginXS` |
| 等宽字族 | `seed.fontFamilyCode`（`SystemFontFamily::ui_monospace`） | 目前未被字体链消费，本 change 接通 |
| 高亮底色 | `mark` 上游硬编码 `gold[2]` | RynUI 以 Typography Component Token 承载固定高亮色，不引入上游硬编码 |

## Risks / Trade-offs

- **等宽字体链的平台差异** → Windows 与原生 Linux 的系统等宽族名称、缺字覆盖与度量不同；两平台分别验证 `code`/`keyboard` 的字形与回退，任一平台的证据不得代替另一方。
- **装饰与字形的坐标系一致性** → 装饰 quad 必须与 glyph 使用同一 origin、clip 和滚动 translation；实现后用滚动/裁剪/DPI 变化的集成测试固定，避免只在静态首帧正确。
- **省略的测量成本与稳定性** → 二分搜索会增加测量次数；要求只在溢出时进入搜索路径，且同宽度重复查询不重复塑形。段落内多行截断与 `expandable` 的交互会改变组件高度，需要确认只触发自身 Measure/Layout。
- **编辑态复用 Input 的视觉继承** → 上游用 CSS 继承把编辑框字体设成被编辑元素的字体；RynUI 需要在编辑态把当前 Typography 的 `SemanticTypography` 传给内部 Input，否则标题编辑会掉回 14px。这要求 Input 暴露内部 typography 入口，属于内部 API 调整。
- **主题 Token 数量增长** → 新增 4 个 map/alias 颜色与 2 组 Component Token 会进入 `theme_runtime` 的 changed-identity 比较；必须确认新增 identity 的 dirty domain 与既有颜色一致（纯 paint/material），不把颜色变化误判成测量失效。
- **焦点与命中顺序** → `copyable`/`editable`/`expandable` 与 `Link` 都会新增可聚焦或可命中元素，必须确认 Tab 顺序、`focus-visible` 呈现、命中裁剪与「父级 disabled 时操作入口仍可交互」的行为明确，且不回归 Button/Input/Search。
- **`Tooltip` 缺口** → `ellipsis.tooltip` 与 `copyable.tooltip` 只有 typed 入口与状态，没有真实浮层；支持范围必须如实标注，不能表述为完整对齐。

## Validation

平台通用部分在 Windows `windows-msvc` Debug 上执行一次并记录 preset：公开 API/头文件隔离合同、标题级别度量与 Token 快照、语义颜色与装饰几何、等宽字族解析与缓存、省略（单行/rows/expandable/宽度重算）、复制（成功/缺端口/失败反馈与 deadline 清理）、编辑（提交/取消/maxLength/disabled/销毁清理）、Link 键盘与指针激活、Divider 几何与方向、主题颜色更新的最小失效与 idle，以及既有 Button/Input/Search/Selection 回归。受影响范围跑定向 CTest，阶段收口跑完整 CTest。

Windows 实机部分在 MSVC x64 + D3D12/DXIL 真实窗口运行 `rynui_token_gallery`：检查五级标题、行内语义、省略展开、复制成功反馈、原地编辑、Link 激活、Divider 各形式在亮色/暗色与系统 display scale 加 acceptance scale 下的可读性与裁切，并记录 driver、shader format、字体、scale、截图、诊断计数与退出码。

Linux 的等宽字族、字体度量与原生 Wayland 窗口检查只在真实 Linux 机器上完成，本 change 不声明 Linux 结果，也不使用 Windows 结果代替。

## Migration Plan

依次提交规划、图标与 Token 基线、平台通用 Typography、平台通用 Divider、Gallery 接入、Windows 实机与集成收口。每个阶段运行该阶段列出的测试后再以英文 conventional commit 提交。现有 `ryn::Text` 公开行为不变，新增 API 不需要迁移；失败时可独立回退对应阶段提交。当前 OpenSpec CLI 拒绝以数字开头的项目约定 change 名，因此规划文件按既有 schema 手工创建，并使用支持该名称的 `openspec validate`；全仓已有 6 个与 034 无关的既有 strict 失败项，本 change 只要求自身 strict 通过。
