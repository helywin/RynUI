# Design

## Context

RynUI 目前只有 `ryn::Text`（content + tone + LayoutStyle），标题层级、行内语义、省略、复制、编辑和 Divider 都不存在。可复用的地基：`TextSceneService` 提供 shape/measure/synchronize 与局部失效，`GlyphScene` 的 quad/glyph 命令进入 `ComponentSceneComposer`，`WindowComponentServices` 统一持有窗口级交互、焦点、动画与 retained surface，`PressableBehavior` 与 `TextEditorStore`/`TextInputSessionHost` 已分别被 Button 与 Input 验证，033 已建立离线图标资源的锁定与验证流程。

本设计在规划评审后做了修正，结论落在「上游数值来源与验证边界」与决策 3、4、5、7、8、9、10、12、14：

1. 语义文字色的 palette 映射写错过（把 palette key 当作相对 step），已改正见决策 9。
2. Token 形状与 `dirty_phase_for` 的固定 phase 模型不兼容，已按字段组重新划分见决策 10。
3. 字体装饰度量、weight/slant face 解析、独立剪贴板绑定与编辑子树生命周期原先被当成现成能力，实际都有运行时缺口，已列为显式前置工作见决策 4、5、8、12。

### 上游数值来源与验证边界

Typography 与 Divider 的 Component Token 默认值、几何规则和行内度量取自锁定 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的以下文件：`components/typography/style/index.ts`、`components/typography/style/mixins.ts`、`components/divider/style/index.ts`、`components/theme/interface/maps/font.ts`、`components/theme/util/alias.ts`、`components/theme/themes/shared/genColorMapToken.ts`。

需要如实说明边界：这些文件**尚未**进入仓库的可重现来源链。`design-tokens/ant-design/6.6.5/sources.lock.yaml` 只锁定 Token 名录涉及的接口文件，`catalog.yaml` 对非 seed Token 一律记录 `upstream_default: null`，`tools/update_ant_design_tokens.py` 只给 seed 写具体值。因此下列数值属于「按锁定 commit 从上游取得」，不是「已由本地基线证明」。落实方式：

- 数值作为 typed adaptation 写入 RynUI Theme，并由 Token 快照测试断言精确 RGBA／长度，不使用「颜色不同」这类弱断言。
- 若要把这些数值升级为可重现来源，必须另立 change 扩展 `sources.lock.yaml` 的逐文件锁定与导入范围；034 内不得声称已完成该扩展。
- 不得用生成器自己写出的 golden 反过来证明上游数值正确。

## Goals / Non-Goals

**Goals:** 建立与 Ant Design 6.6.5 一致的公开排版与分割线能力；装饰、省略、复制、编辑全部复用既有文本、交互与剪贴板路径；主题变化只产生最小失效；交互元素遵守既有焦点、命中与销毁合同。

**Non-Goals:** `Tooltip` 浮层本体（`ellipsis.tooltip` 与 `copyable.tooltip` 只发布 typed 入口与状态，真实浮层留给后续 Tooltip change）；`Typography` 的 `setContent` 级联排版与 `ul`/`ol`/`pre`/`blockquote`/`table` reset；富文本；`TextArea` 多行编辑；任意值类型的编辑回调；`copyable.format` 同时写入 HTML；本 change 未列出的额外线型。

## 决策
### 1. 两个独立组件宿主，各自复用窗口服务

新增 `TypographyComponentHost` 与 `DividerComponentHost`，都以 `WindowComponentServices&` 构造并实现 `WindowComponentParticipant`，沿用 `PressableBehavior`、`InteractionRegistry`、`FocusManager`、`AnimationRuntime` 与 retained surface。不把两者塞进 Button 宿主，也不建立新的窗口级单例。备选是继续扩张 `ButtonComponentHost` 的兼容宿主，会让排版组件依赖按钮语义。

### 2. 行内语义是 Typography 的属性，不是子组件

`strong`/`italic`/`underline`/`delete`/`code`/`keyboard`/`mark` 通过 typed Props 表达，因为它们的可见效果作用在同一段被塑形的文本上。拆成嵌套组件就必须实现通用 inline style 合并与文本 run 切分，超出本 change。

### 3. 装饰分两层绘制，背景必须先于字形

命令顺序在 `ComponentSceneComposer` 中被保留，且 `ComponentHost` 已提供 `before_children`/`after_children` 分层：

- **glyph 之前**：`mark` 高亮底、`code` 底色与边框、`keyboard` 底色/边框/2 倍下边框。不透明或半透明填充若画在字形之后，会覆盖或染色文字。
- **glyph 之后**：`underline` 与 `delete` 装饰线，只使用文本当前前景色。

装饰几何取自测量结果而非估计值：位置来自 `TextMeasurement` 的逐行 `baseline`/`width` 与该行 run 的字体装饰度量；多行段落逐行绘制。宽度、字号或文本变化后装饰与文字在同一轮同步里一起重算。

### 4. 装饰度量需要扩展字体度量（前置工作）

`font::FontMetrics` 目前只有 ascent/descent/line_gap/size/scale，**没有** underline/strikeout 的位置与厚度，`underline` 与 `delete` 不能凭空猜位置。决定：扩展 `font::FontMetrics` 的装饰字段。规划阶段曾写成「FreeType 在 `FT_Face` 上直接提供四个字段」，实施时核实这是错的：`FT_FaceRec` 只有 `underline_position` 与 `underline_thickness`，strikeout 必须通过 `FT_Get_Sfnt_Table(face, FT_SFNT_OS2)` 读 OS/2 表的 `yStrikeoutPosition`/`yStrikeoutSize`（font units）。

四个字段统一以 **em 相对值**保存，且**全部通过 `units_per_EM` 归一化**。这里有两处规划／实施错误，均已修正：

1. 规划阶段曾写成「FreeType 在 `FT_Face` 上直接提供四个字段」。`FT_FaceRec` 只有 `underline_position` 与 `underline_thickness`，strikeout 必须通过 `FT_Get_Sfnt_Table(face, FT_SFNT_OS2)` 读 OS/2 表的 `yStrikeoutPosition`/`yStrikeoutSize`。
2. 实施阶段我先按「FreeType 把 underline 值缩放到 26.6 单位」写了除以 `64 × raster_pixel_size` 的换算，**这是错的**，由 codex 独立审查发现。锁定到本地的 FreeType 2.14.3 头文件明确写着 `underline_position`/`underline_thickness` 的单位是 **font units**，与 `units_per_EM` 同坐标系；`fixed_26_6_to_pixels` 只适用于 `FT_Size_Metrics` 上的 26.6 字段。错误换算会额外除以 64，使下划线位置与厚度偏小约一个数量级并随像素尺寸漂移。

坐标约定冻结为：位置与厚度均**相对 baseline 向上为正**（与 font space 及 `ascent` 一致）；绘制到屏幕坐标（y 向下增长）时用 `baseline - position * font_size`。`units_per_EM` 不可用的 face 四个字段保持 0，缺 OS/2 表的 face `strikeout_*` 保持 0；厚度为 0 时装饰跳过，而不是在 baseline 上画一条发丝线。

测试必须能证伪换算错误：旧公式的偏差在固定像素尺寸下是**系统性常数**，宽松绝对容差抓不住（原测试用 `0.05F` 容差与「非负」检查，因此放过了错误实现）。改为断言三条独立性质——同一 face 在两个逻辑像素尺寸下的 em 比值相等、em 比值等于「像素差值 ÷ 字号差」、数值落在真实字体表使用的区间内（下划线位置约占 em 的 -0.02～-0.30，厚度 0.01～0.20）。已用「临时恢复错误公式 → 新测试报 `underline position is not em-relative`」验证该断言确实可证伪。

备选是用 ascent/descent 派生近似位置，会在不同字体与 DPI 下明显偏移，不采用。

### 5. 字重与斜体需要真实 face 解析（前置工作）

评审确认：默认 resolver 忽略 weight，Windows 初选固定 NORMAL weight/style，`SemanticTypography` 与 `ThemeFontResolver` 都没有 slant。因此仅增加 `(font_family, pixel_size)` 缓存**不会**让 `strong`/`italic` 生效。决定：

- `ThemeFontResolver` 签名扩展为 `(family, weight, italic, pixel_size)`，`runtime::SemanticTypography` 增加 `italic`，缓存键为四元组并按需惰性解析（不预载 weight×slant 矩阵）。
- 平台解析统一为 `platform_styled_descriptor(family, weight, italic)`。Windows 枚举 family 的字体列表并按 weight/style 距离选取：**实施时核实 `GetFirstMatchingFont`（含按 weight 请求）对可变字体（Segoe UI Variable）会对每个 weight 返回同一文件**，因此按原设计直接请求 weight 会让 bold 静默复用常规 face。Linux 通过 Fontconfig 的 `FC_WEIGHT`/`FC_SLANT` 匹配并回传真实 style 供调用方判定。
- 取不到对应 face 时回退常规 face，并把精确原因写入 `DefaultFontChainResult::diagnostic_fallbacks`；styled face 只 FRONT 在常规链之前，等宽链仍追加 UI 链，保证覆盖不下降。
- `strong` 使用 `alias.fontWeightStrong`（600）；`italic` 请求 italic face。
- `code`/`keyboard` 使用 `TypographyThemeToken::font_family_code`，并在 `ThemeScope` 新增独立的 `code_font_family()` accessor；**不得**复用 `text_font_family()`（它只返回 Text 字族）。
- 已知边界：Windows 的 `latin_families`/`monospace_families` 是具名族优先列表，只有这些具名族按 weight/slant 请求；Linux 的 `sans-serif`/`monospace` 是 Fontconfig 别名、无法枚举，故交由 `FC_WEIGHT`/`FC_SLANT` 匹配。两平台的实测结果必须各自记录，不得互相代替。

### 6. 等宽字族走参数化字体链

`make_default_ui_font_resolver` 现在忽略 `SystemFontFamily` 参数（形参未命名）。改为按字族与像素尺寸分别缓存：`ui_sans` 沿用现有首选/系统回退/CJK 回退，`ui_monospace` 增加各平台等宽首选族（Windows Consolas/Cascadia Mono、Linux DejaVu Sans Mono/Noto Sans Mono）并复用同一缺字回退与验证字体兜底。`DefaultFontChainRequest` 增加等宽首选入口。`ui_monospace` 的缺字回退仍是链中可覆盖该字形的 face（例如 CJK 注释），`code` 因此逐字素混排，不使用系统符号近似。

### 7. 省略按字素边界收缩，并修正 overflow 假设

评审纠正了两处错误假设：

- **单行截断不能用「测量溢出」触发。** 有限宽度下 engine 会换行，正常换行行的 `overflow` 为 false，只有单个 cluster 放不下才为 true；长句可能多行而总 `overflow` 仍为 false。单行截断改判据为「自然宽度（无宽度约束测量）超过可用宽度」或 `lines.size() > 1`。
- **多行判断用 `lines.size() > rows`**，并在最后一行留足省略后缀与操作入口（`copyable`/`editable`/`expandable`）所需宽度。

字素边界复用 `TextBoundaryMap::grapheme_bytes`，不得把 HarfBuzz cluster 或 UTF-8 scalar 当字素。候选测量需要独立于 retained scene：新增一个不发布到 glyph scene 的测量通道，并在同一轮内缓存候选结果；`set_content` 会 invalidate shape，所以候选文本查询不能依赖「已同步缓存」自动消除重新塑形。实现必须给出 `shape_count`/`measure_count` 合同，明确哪些查询可复用、哪些必然 reshape。

退化行为已冻结为精确场景表（决策 19 定稿，规格 `Degenerate ellipsis inputs` 同步），**不再**使用早期「`rows == 0` 只留省略后缀」的表述——该表述与规格的「保留最小可见内容」冲突，且会让 `rows == 0` 显示一个没有内容的省略后缀。冻结结果：`rows == 0` 或可用宽度 `<= 0` 不产生行盒；行数超限但末行放不下后缀时**不加后缀**（后缀永远不作为可裁内容）；末行放不下任何完整字素时产生空行盒；显式换行保留为行结构，截断只作用于行数。完整表见规格。

### 8. `copyable` 的剪贴板必须能独立于文本输入绑定

评审确认：全仓生产代码中 `bind_text_edit` 的唯一调用方是 `InputComponentHost` 构造，普通 `WindowComponentServices` 构造不绑定端口。因此纯 Typography 窗口会永远走缺端口分支，而 Gallery 因为有 Input host 会掩盖这个缺口。决定：

- 由窗口 adapter 显式提供剪贴板端口，使其**不依赖**文本输入或 editor 服务：`WindowComponentServices` 增加独立的 clipboard 绑定入口，内部只服务 `TextClipboard::write_text`/`has_text` 需求，不复用为 editor owner 设计的 `TextClipboardCommands`。
- 保留可空语义与晚绑定通知：`bind_clipboard` 在已有挂载参与者时通知它们，参与者通过新增的、带默认实现的钩子刷新可用性。
- 验收必须包含一个**没有任何 Input 组件或 host** 的 copyable 真实窗口；不得只用 Gallery 结果声称已接通。

### 9. 语义文字色按相对 step 派生（评审修正）

Ant Design 的 `genColorMapToken` 用 `generateColorPalettes(base)` 的 **palette key** 取色：`colorXxxText = palettes[9]`、`colorXxxTextHover = palettes[8]`、`colorXxxTextActive = palettes[10]`。

RynUI 的 `palette_variant(seed, step, light)` 已实现 `@ant-design/colors` 8.0.1 同一算法，但它的 `step` 是**相对 key 6（seed 本身）的偏移**，不是 key。以默认 seed 实测（`success #52c41a`、`warning #faad14`、`error #ff4d4f`）：

| step | success | warning | error |
| --- | --- | --- | --- |
| 1 light | (115,209,61) | (255,197,61) | (255,120,117) |
| 3 light | (183,235,143) | (255,229,143) | (255,204,199) |
| 9 light | **(249,255,240)** | **(255,254,240)** | **(255,244,240)** |

step 9 会把饱和度按 `-0.16 × 9` 夹到 0.06、value 按 `+0.05 × 9` 夹到 1，结果是近白色。所以**不能**把 key 当 step 传入。

按当前实现「seed 即 key 6」的约定，key 8/9/10 对应**暗色侧**的相对 step 2/3/4（亮色侧 step 越大越浅，取不到 key 8/9/10）。因此三个语义色统一按暗色侧 palette 派生：

- `hover = palette_variant(seed, 2, false)`、`text = palette_variant(seed, 3, false)`、`active = palette_variant(seed, 4, false)`。
- 与 `derive_input_theme` 的既有约定一致，先取暗色侧 palette 再混到容器底色：亮色容器取 `mix(color_bg_container, palette, 1.0)`（即 palette 值本身），暗色容器取 `mix(rgb(20,20,20), palette, 0.85F)`。`genColorMapToken` 是拿 seed 计算的纯函数，与主题算法无关，所以暗色侧固定使用该配方而不随算法改变。
- 亮色侧派生值同时是 `colorXxxHover`/`colorXxxActive`（key 5／key 7 语义，即现有 `semantic_palette` 的 step 1 light／step 1 dark），**不得改动**这两个已发布值；快照测试必须证明已使用的 step 1–7 取值不变。

`colorLink`：新增 `ThemeMapToken::color_link`，`colorLink = seed.colorLink ?? seed.colorInfo`（上游 `genColorMapToken`），取 key 6 即 base 本身；`colorLinkHover`/`colorLinkActive` 取暗色侧 step 2／4。`AntDesignDefaultSeed` 已有 `optional<Color> color_link`，但 `ThemeMapToken` 一直缺 link 字段，本 change 补齐。

`colorSplit = getAlphaColor(colorBorderSecondary, colorBgContainer)`。RynUI 已有一个实现同一上游 alpha 搜索的 `outline_color` lambda（现位于 `derive_input_theme` 内）。决定：提取为可复用的 foreground/background helper，alias 与 Input 阴影共用同一份实现，不复制两份可能漂移的搜索逻辑；提取后必须用现有 contract 测试证明三种 outline 与 Dark/custom seed 的精确 RGBA 不变。

`colorTextHeading`、`colorTextDescription`、`colorTextDisabled` 分别等于 `colorText`、`colorTextTertiary`、`colorTextQuaternary`。RynUI 已有 `color_text`（heading/正文主色）、`color_text_secondary`（对应 `colorTextDescription`）与 `color_text_disabled`，`TypographyThemeToken` 复用它们，不新增重复字段。

### 10. Token 形状按 dirty domain 分组（评审修正）

`dirty_phase_for(identity)` 对每个 identity 返回**固定** phase，`collect_changed` 按 identity 分组比较。因此 token 结构的字段分组必须与 phase 一一对应，否则必然出现「颜色变化触发重新测量」或「线宽变化漏测量」。用嵌套结构显式表达分组，组内字段共享同一 phase。

**Typography**

| 组 | 字段 | phase |
| --- | --- | --- |
| `heading_font_sizes[5]`／`heading_line_heights[5]` | 五级标题字号（38/30/24/20/16）与行高比（1.4/1.35/1.3/1.25/1.2） | `text \| measure_layout` |
| `font_family`／`font_family_code`／`font_weight`／`font_weight_strong` | UI 字族、等宽字族、常规与强调字重（400／600） | `text \| measure_layout` |
| `base_font_size`／`base_line_height` | `fontSize`／`lineHeight`，标题与 code/kbd 按比例解算 | `text \| measure_layout` |
| `colors.*` | `text`、`description`、`success`、`warning`、`error`、`disabled`、`link`、`error_text_hover`、`error_text_active`、`mark_background` | **仅** `paint_material` |
| `code.*`、`keyboard.*` | `font_scale`、`padding_inline_em`、`padding_block_start_em`、`padding_block_end_em`；code 另有 `background`／`border_color`／`border_width`／`border_radius`，keyboard 另有 `background`／`border_color`／`border_width`／`border_bottom_width`／`border_radius` | `measure_layout \| geometry`；底色属于同一视觉单元，故整组按度量处理，并在测试中固定该取舍 |
| `title_margin_top`（`1.2em`）／`title_margin_bottom`（`0.5em`） | 标题上下间距，相对标题自身字号的 em 比例 | `measure_layout \| geometry` |

单位约定：`*_em` 与标题行高是**比值**，按实际字号解算；override 中不接受裸 `LogicalLength` 当作 em。五级标题提供 `heading_font_sizes`／`heading_line_heights` 的逐级 override。

**Divider**

| 组 | 字段 | phase |
| --- | --- | --- |
| `colors.*` | `line`（`colorSplit`）、`text`（`colorTextHeading`）、`plain_text`（`colorText`） | **仅** `paint_material` |
| `metrics.*` | `line_width`、`orientation_margin`（`0.05`）、`text_padding_inline`（`1em` 解算值）、`vertical_margin_inline`（`marginXS`）、`horizontal_margin`（`marginLG`）、`horizontal_with_text_margin`（`margin`） | `measure_layout \| geometry \| hit_test` |
| `typography.*` | `text_font_size`（`fontSizeLG`）、`text_font_weight`（500）、`plain_font_size`（`fontSize`）、`plain_font_weight`（常规） | `text \| measure_layout` |

`identity` 相应拆为 `typography_colors`／`typography_typography`／`typography_metrics` 与 `divider_colors`／`divider_metrics`／`divider_typography`；`collect_changed` 按字段组比较，一个已变化 identity 只 append 一次。`alias_color_split` 进入 alias 组，phase 为 `paint_material`。

### 11. 主题扩展沿用既有 Component Token 模式

`ThemeMapToken` 增加 `color_success_text`／`color_warning_text`／`color_error_text`／`color_link`，`ThemeAliasToken` 增加 `color_split`；新增 `TypographyThemeToken` 与 `DividerThemeToken`、对应 `ThemeConfig` override、`theme_runtime::TokenIdentity` 条目、`dirty_phase_for` 映射、`collect_changed` 比较、`snapshot_identity` hash、`serialize_snapshot` 输出与 `operator==`。`ThemeScope` 增加 accessor；裸读 `snapshot()` 不建立 token capture，不能代替 accessor。

五个 golden（`default`/`dark`/`compact`/`dark-compact`/`compact-dark`）都必须重新生成，因为 hash 覆盖新增字段后五份 identity 全部改变；既有字段的旧值必须保留，以便捕捉旧 palette/step 回归。

### 12. `editable` 预建编辑子树，不做点击后动态挂载（评审修正）

评审确认当前运行时**没有**点击后新建子树的路径：`WindowComponentServices::mount` 只在调用期间运行各 participant 的 `begin_mount`/`end_mount`，Input 在其中注册 `thread_local` 后恢复；`ComponentHost` 的 `create_record`/`mount_slot` 只允许首次 mount，整个 Host 还禁止第二次 mount；公开 `ryn::Input` 在 `active_input_host` 为空时抛异常。因此原决策「激活时构建真实 Input」不成立。

改为：**首次挂载时在同一 typed slot 预建编辑子树**（真实 `ryn::Input`，受 `LayoutStyle`/可见性控制），编辑态只切换布局、可见性与 eligibility 及焦点，不新建结构。实现约束：

- 编辑输入框复用同一 `WindowComponentServices` 的文本编辑服务，不二次绑定端口。
- 编辑态继承被编辑元素的 `SemanticTypography`：Input 需要接受父 slot 传入的字族/字重/字号/行高，不能让标题编辑掉回 14px。这是内部 API 扩展，属前置工作。
- `Input` 目前只有 `onChange`/`onSubmit`，**没有** blur/cancel 回调；失焦 handler 只停光标与 blur session，Esc 在没有 composition 时直接返回 false、有 composition 时只取消 composition。需要补内部编辑生命周期回调，并冻结 Esc 对「IME composition」与「编辑草稿」两层的优先级。
- 键盘激活与 focus state 回调都在 `FocusManager` dispatch 内同步触发，此时 `request_focus`/`clear_focus` 会被 `begin_operation` 拒绝（019 已记录同一限制）。需要新增「dispatch 之后执行、校验 generation」的焦点事务，不能把请求塞进 handler 内同步执行。

受控语义需要冻结：受控 `content` 与编辑草稿冲突时以草稿为准并等待外部回写；外部拒绝回写时保留草稿并保持编辑态还是退出，必须明确并测试。

### 13. 省略与复制/编辑组合的语义

`ellipsis` 与 `copyable`/`editable` 同时启用时，**复制的是原始全文**（不是截断或展开后的显示文本），编辑初始值也是原始全文。`expandable` 的展开/收起是显示状态，不影响复制与编辑的取值来源。这条进入 spec 场景。

### 14. Divider 行为冻结（评审修正）

原 spec／design／tasks 互相矛盾，冻结如下：

- **垂直高度基准**：垂直分割线沿用上游 `0.9em` 相对高度的语义，但以**当前行高**为基准解算为 fixed 长度，并保留 `top: -0.06em` 的相对偏移；spec 中「延续整行可用高度」的措辞删除，改为「使用相对当前行高的固定高度，不撑高或压缩相邻内容」。
- **`orientation_margin` 语义（二次修订）**：区分两个来源。Theme `DividerThemeToken::metrics.orientation_margin` 是 Component Token 默认值（`0.05`）；组件 prop `orientationMargin` 是显式覆盖。**显式组件比例覆盖 Theme 默认比例**：`Theme` 使用 Token 比例，显式比例（含 0）使用 prop；当显式取 `None` 时才按上游 `no-default-orientation-margin` 规则把该侧轨道宽度归零、并把 `text_padding_inline` 换成 `sizePaddingEdgeHorizontal`（`0`）。三态 `Theme`/`None`/显式比例必须分别编码，「0」与「未设置」不得混用。本节原先写的「prop 未设置即归零」与规格冲突（默认 0.05 下两者给出不同轨道宽度与 padding），已按规格统一。
- **`plain` 语义（二次修订）**：只改**文字**（`plain_text` 色、常规字重、`plain_font_size`），不改线色、线宽与轨道规则；但允许文字度量变化引起布局重算（字号变化会改变标签宽度与整体高度）。spec 中「更浅的填充色」与「线条几何保持不变」的绝对表述一并删除。
- **`disabled`**：本 change **不提供** `Divider.disabled`。Divider 没有交互状态，proposal／spec／tasks 中相关表述一律删除。

#### 实施时核实的约束（影响 Divider 的测量方案）

1. **`LeafLayout` + intrinsic measure 不会先测量子节点。** `LayoutEngine::measure_node` 对 `LeafLayout` 直接调用 intrinsic 回调，只有 `BoxLayout`/`FlexLayout` 才先 `measure_node` 各子节点。因此若把标签作为分割线的子节点、同时给分割线挂 intrinsic measure，回调里**读不到标签的已测宽度**。
2. **`BoxLayout` 拿到尺寸不等于完成放置。** 它只提供 child-first measure，不执行父 intrinsic 回调，且把所有子节点放在内容区左上角，因此标签与两条轨道的分配必须另行设计。
3. **父辅助几何同步晚于 Text 同步。** 在辅助阶段只改标签 Node bounds 会让当帧字形留在旧位置。

以上三条的最终落点见决策 23。

因此带标签的水平分割线的落地路径是二者之一，必须在实现前选定并在 `tasks.md` 中体现：

- **路径 A（推荐）**：有标签时用 `BoxLayout`／`FlexLayout`，由布局引擎测量标签节点并承担轨道与整体高度的组合；无标签时用 `LeafLayout` + intrinsic measure 产出贯通全宽的单条轨道。切换标签内容时布局模型随之切换，需要显式重挂载或模型切换事务。
- **路径 B**：保留 `LeafLayout` 单节点，用「缓存标签测量 + 版本修正」跨帧收敛整体高度，代价是首帧高度可能不准，且需要额外的版本账本。

两条路径都必须覆盖：无标签贯通全宽、`left`/`right`/`center` 的轨道比例、`orientationMargin` 的 `Theme`/`None`/显式比例三态、垂直分割线的固定高度与行内间距、`plain` 只改文字，以及颜色变化只产生材质失效。

### 15. `Link` 交互按 Button 现有模式实现

`Link` 接入 `InteractionRegistry` 的 eligible/focusable、`FocusManager` 的 Tab/Enter/Space 与 `PressableBehavior` 的指针手势与 capture，与 Button 同一路径；`disabled` 同时取消 capture、清除 eligibility、清理焦点并更新 hit snapshot。评审确认普通 Link 点击**不需要**新增导航 API；需要延迟焦点事务的只有 `editable` 这类内部结构与焦点切换（见决策 12），不得把 019 的限制误报成所有 Link 都不可实现。

### 16. 图标资源沿用 033 的锁定流程

新增 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 五张官方 SVG 到 `third_party/ant-design-icons`（`@ant-design/icons-svg` 4.6.0，锁定 commit 与许可证），扩展 `tools/generate_icon_assets.py` 图标表并重新生成 `src/icons/ant_design_icon_font.inc` 与 manifest；`tools/verify_icon_assets.py` 的固定数量从 9 改为 14。该项已完成并提交。

### 17. Gallery 与参考数据同步

`ant.component.typography` 与 `ant.component.divider` 增加真实样例，support overlay 的 `status`、`supported_scope`、`missing_scope`、`evidence_identifiers` 与 reference catalog 合同同步更新，`missing_scope` 明确保留 `tooltip` 浮层等未覆盖项。`tools/update_ant_design_tokens.py` 只在相应能力**真实接通**后提升 support，不提前提升 hover/active/Link 或未实现的渲染支持；本次新增的 C++ 私有 code/kbd 字段不得冒充新增上游 Component Token。

### 18. 装饰需要独立的 quad 归属与可变数量（4.3，二次修订）

**排序机制成立，数据归属不成立。** 已核实的部分：`ComponentHost::Record` 同时持有 `before_children_fragments` 与 `after_children_fragments`，`register_scene_fragment` 每次调用都追加新片段、没有「一组件一片段」限制，`append_paint_subtree` 按「前置片段注册顺序 → 子组件 → 后置片段注册顺序」遍历。因此「背景先于 glyph、装饰线后于 glyph」可以用三个片段（背景、glyph、装饰线）表达，且各自独立更新。

但 `SceneDrawCommand` 只有实例索引与数量（`src/graphics/glyph_scene.hpp:160-164`），**不是实例存储**；窗口实际上传的是共享 surface store（`examples/token_gallery/token_gallery_runtime.cpp:646-655`）。另外 `RetainedSurfaceService` 单条 surface 上限 16 个 quad，且 `update_surface` 不允许数量变化（`src/component/retained_surface_service.hpp:18`、`retained_surface_service.cpp:146-155,447-453`）——多行装饰在 resize/重排后会超限并抛异常。决定：

- **装饰 quad 必须与窗口共享同一个 quad store。** 在 `TextComponentHost` 上增加一个可选的窗口级 quad 内容服务引用（由 `WindowComponentServices` 在构造时注入），装饰 quad 写入该服务、与该窗口其他组件的 quad 共用同一 GPU 上传缓冲；**不新建** Text 私有的未接入上传的 store。
- **服务必须支持可变长度的 range 并重映射。** 扩展 `RetainedSurfaceService`：允许 `update_surface` 改变实例数量，并在 range 增长/收缩时重映射所有受影响的后续记录（已有的 `fragment_remaps` 计数器正是为此存在）。不这样做就无法支持换行/重排后装饰数量变化。
- **每个装饰片段只有一个发布者。** 背景片段由装饰服务发布、glyph 片段由 `TextComponentHost::sync_fragments` 发布、装饰线片段由装饰服务发布；三者的 fragment identity 保持独立，不共享同一个 fragment 后各自「追加」（`set_fragment` 是完整覆盖语义，`retained_surface_service.cpp:374-387`、`text_component.cpp:592-596` 都依赖这一点）。
- 几何取自 `TextMeasurement` 的逐行 `baseline`/`width`/`content_bounds`，线的位置与厚度取自该行 run 的 `font::FontMetrics`（决策 4 已修正为 em 相对），按 `font_size` 换算；`mark` 只覆盖 `content_bounds` 的水平范围。多行段落逐行产生装饰 quad，因此数量可变是常态而非例外。
- 纯 `Text(String)` 与 Icon 不注册装饰片段，保持零开销。
- **颜色与度量必须分开失效。** Typography 规格要求纯颜色更新不触发测量，而 `code`/`keyboard` 的背景色与度量目前同属一个 `measure_layout|geometry` identity。决定：把 inline 背景/边框颜色与 inline 度量拆成不同 identity，颜色变化只产生材质失效；装饰线颜色同样只产生材质失效。实现必须通过值差异保证这一点，不能接受「整组按度量处理」作为取舍。
- **订阅缺口必须一起补。** 4.2 已使用 `code.font_scale`/`keyboard.font_scale`，但 `subscribe_theme` 当时未捕获 `typography_inline_code`/`typography_inline_keyboard`，只改 `font_scale` 不会通知任何 code/kbd 组件。已补捕获并加入会失败的回归测试；后续装饰接线必须沿用同一捕获纪律。
- 备选是把背景交给子节点、线留给父节点。否决理由：一个语义组件会对应多个组件记录，命中测试、主题订阅、销毁路径都要复制，且 `component_scene.cpp:35-46` 要求 interaction 与 fragment 属于同一组件，跨记录无法满足。

### 19. 省略需要完整的配置、结果与失效协议（4.4，二次修订）

**落点判断成立。** 已核实：`text_engine.cpp` 的塑形只产生 `ShapedText`，真正生成/替换 glyph 实例发生在 `glyph_scene.cpp:386-414`，由 `text_scene_service.cpp:418-426` 调用，所以候选塑形不污染 retained scene；字素边界存在，但在 `ryn::input::TextBoundaryMap`（`src/input/text_boundary.hpp:34-45`）而非 text engine 内部。

**未闭合的是协议。** `TextState` 只有内容、字体、行高与 `max_width`（`text_engine.hpp:195-242`），`TextSceneService` 也没有代理配置的接口，`TypographySemantics` 同样没有这些字段。决定：

- **配置协议**：`TextState` 增加省略配置——`rows`（`0` 表示不限制）、`suffix`（省略后缀，默认取 `…`，并记录其字形可用性）、`expanded`（展开态）、`reserved_inline`（末行必须为 copy/edit/expand 入口预留的行内宽度）。`TextSceneService` 提供对应的代理方法，任一项变化都要更新 intrinsic revision 并产生 layout/geometry 失效，不能只改 `TextState` 而跳过 scene 账本。
- **保留两份塑形**：始终保留「原始内容 + 自然塑形」与「最终显示塑形」两份结果。宽度增大时从**全文**重新搜索，不能继续对上一轮已截断的 `shaped_` 测量（当前 width setter 只失效 layout，不足以支撑这一点）。展开态返回全文与其自然测量。
- **后缀缺字**：塑形走 fallback 查找时可能替换为 U+FFFD 或报 `missing_glyph`。决定：后缀字形在解析出的链中逐个 face 查找，找不到则退化为**不加后缀**并在 `TextState` 上暴露可观察标志，不使用 U+FFFD 作为省略号。默认后缀必须在测试里固定为实际可见的省略字形。
- **可观察合同**：`TextStateCounters` 增加 `ellipsis_searches` 与 `ellipsis_shapes`，明确各自包含哪些操作；测试断言「同宽度重复查询不新增塑形」与「宽度增大后从全文重搜」。
- **搜索上界不作为已验证事实。** 重新塑形后的候选宽度对字素前缀未必严格单调（当前直接调用 HarfBuzz，没有单调性合同），因此实现采用「二分定位 + 线性回退确认最长可容前缀」，测试固定的是**结果正确性**（最长可容字素前缀）与「同宽度幂等」；`log2(N)+1` 只作为期望量级记录，不作为验收断言。
- **退化行为必须与规格逐场景对齐。** 决策 7 的「`rows == 0` 只留后缀」与 Typography 规格「无后缀时保留最小可见内容或保留显式换行结构、禁止空场景」不是同一个输出。实现前必须写出精确场景表（`rows` 为 0/1/N、有无后缀、是否有显式换行、可用宽度小于后缀时各自的结果），并同步决策 7 与规格，不能两边各写一套。

### 20. 剪贴板必须有独立于文本编辑的绑定入口（4.5，二次修订）

**我上一版自相矛盾**：`bind_text_edit` 必然创建 `TextEditorStore`、session host 与 `TextClipboardCommands`（`src/component/window_component_services.cpp:105-112`、`window_text_edit_services.hpp:12-14,22-31`），且原始 clipboard 指针是私有的——这正好违反决策 8 与我自己的「不创建 `TextEditorStore`」。决定：

- **新增独立的 `bind_clipboard(clipboard)`**，与 `bind_text_edit` 并列，不创建 editor store、不创建 session host。窗口 adapter（Gallery 与各示例 runtime）在启动时调用它。
- 暴露 owner-thread 的可空端口访问或 `write_clipboard_text` 服务，以及只读的「端口已绑定」查询。**可用性查询不能用 `has_text().available` 代替写能力**：`has_text` 表示剪贴板当前有无文本（`src/input/text_clipboard.hpp:22-31`），空剪贴板仍然必须允许复制。
- `Input` 的 `bind_text_edit` 可以复用同一端口，但两者互相独立：只绑 clipboard 不绑 editor 的窗口，排版组件仍能复制。
- **晚绑定通知必须显式遍历，不能用 `begin_mount`/`end_mount`。** 它们只围住首次同步 `mount`（`window_component_services.cpp:119-137`），晚绑定不会再触发。改为：`bind_clipboard` 完成后由 `WindowComponentServices` 主动遍历 participant，participant 刷新已挂载记录的可用状态与反馈并请求必要帧。
- 未绑定的窗口完整渲染与排版，复制入口不可用且可观察，不抛出。测试覆盖「已绑定」「未绑定」「完全没有 Input 组件与 host」三种；真实窗口的复制行为属于 Windows 7.1 的真实窗口验收，不以通用 headless 用例替代。

### 21. `editable` 的容器布局、活动状态与继承行高（4.6，二次修订）

**三个基础判断已核实成立**：semantic typography 可以经 typed slot 携带响应式 Prop（`component_host.hpp:241-252,276-279`）；Input 当前只有 `onChange`/`onSubmit`，没有独立 commit/cancel/blur（`include/ryn/input.hpp:34-35,48`），Esc 有 composition 时取消 composition、否则返回 false；`FocusManager` 在整个 dispatch 内保持 operation 标志，同步 `request_focus`/`clear_focus` 会抛重入异常（`focus_manager.cpp:23-25,167-195,307-320`）。

**我上一版把问题简化了。** 只传字体 Prop 不足以让标题可编辑，还有三处必须一并解决：

- **继承行高会改变控件高度。** Input 的 `control_height` 与 padding 来自固定控件尺寸（`input_component.cpp:674-693`），`InputContentLayout` 用它限制 editable viewport 高度（`layout_engine.cpp:810-825`）。标题行高更大时必须**派生**足够的内部 `control_height`/viewport，否则文本被裁切。字体 Prop 还必须覆盖 `update_theme` 对字体的回写。
- **容器布局要明确。** Typography 当前是 `LeafLayout`（`text_component.cpp:988-990`），不会测量或放置子节点。决定：可编辑排版组件改用能表达「显示分支／编辑分支二选一」的持久布局模型，只测量并放置当前活动分支；两个分支的节点常驻，通过内部状态切换，不增加公开 `LayoutStyle` 的可见性字段（`LayoutStyle` 只管外部布局，`node_store.hpp:64-80` 也没有 visibility）。
- **活动状态必须统一关闭。** 仅改 opacrity 或缩小父布局不会停用 Input 的独立绘制、焦点、IME、caret。决定：`InputComponentHost` 增加内部的 active/suspended 状态，统一关闭 interaction eligibility、pointer capture、edit session、caret deadline 与全部绘制层。
- **字体提取**：`InputComponentHost` 增加内部入口，允许挂载方为一个输入提供 `Prop<runtime::SemanticTypography>`，与 Text 的 `semantic_typography` 走同一条通道。
- **编辑生命周期**：增加内部 `on_commit`/`on_cancel`/`on_blur`（不进入公开 API）。Esc 优先级：有 IME composition 时先取消 composition 并保持编辑态，再次 Esc 才放弃草稿。
- **焦点事务**：`FocusManager` 增加「请求队列 + 派发结束后刷新」。刷新在 `begin_operation` 作用域之外执行，应用前校验 **`InteractionId` 的 generation 与 registry eligibility**（`focus_manager.cpp:333-335`、`interaction_registry.cpp:96-108`），而不只是 `ComponentId`。这与 019 记录的根因相同，修在焦点管理器里。
- 受控语义：草稿优先于受控 `content`；提交后等待外部回写，外部拒绝回写时保留草稿并保持编辑态，且状态可观察。该行为必须有测试。

### 22. Link 用 build context 内的 slot composition（4.7，二次修订）

**我上一版的挂载路径会抛异常。** `TextComponentHost::mount` 调用的是整个 `ComponentHost::mount`（`text_component.cpp:393-399`），而 `ComponentHost` 同时禁止重入与二次 mount（`component_host.cpp:89-94`）；另建一个 `TextComponentHost` 又会带来第二棵 `ComponentHost`，无法使用窗口 composer 的 fragment/interaction identity。决定：

- `LinkComponentHost` 作为 `WindowComponentServices` 参与者（照 `SelectionComponentHost` 的模式：注册 participant、代理全窗口 mount），在**首次 mount 的当前 build context 内**创建 Link root 与自己的 interaction，再通过 `mount_slot_with_semantic_text_style` 声明一个 `Text` 子组件，由已有的 `active_text_host` 消费。不在 builder 内调用 `TextComponentHost::mount`。
- **interaction 必须绑定 Link 自己的 fragment。** `component_scene.cpp:35-46` 要求 interaction 与 fragment 属于同一组件，因此不能把父 Link 的 interaction 填到子 Text 的 glyph fragment 上；窗口文本同步本身固定不绑定 interaction（`window_component_services.cpp:219-220`）。Link 用一个自己的 fragment（命令列表可为空）承载 interaction，子 Text fragment 只发布 glyph。
- 交互合同与 Button 一致：`InteractionRegistry` 的 eligible/focusable、`FocusManager` 的 Tab/Enter/Space、`PressableBehavior` 的指针手势与 capture；`disabled` 时取消 capture、清除 eligibility、清理焦点并更新 hit snapshot。
- `resolve_fonts` 只用于解析字体链，不用于声明子组件。

### 23. Divider：稳定布局模型 + 引擎测量标签，禁止重挂载（二次修订）

**上一版不可行。** 它要求「标签空/非空切换时重挂载」，但 `ComponentHost` 明确禁止二次 mount 与挂载后声明子组件（`component_host.cpp:92-94,281-282,371-372`），而 Divider 规格的 `Reactive divider properties` 又要求这类更新保持 identity。同时决策 14 记录的「标签宽度只能在文本同步后得知」也已不成立：`Text` 的 intrinsic 回调确实能在 measure 阶段同步完成 shape/measurement（`text_component.cpp:888-898`）。修订如下：

- **标签节点常驻，布局模型可切换。** 首次挂载就建好标签节点与轨道几何，标签从空变非空（或反向）时用 `LayoutEngine::set_layout` 在两种模型间切换，**不销毁重建**，root/component/fragment identity 全程保持。
- **标签尺寸由引擎测量，轨道与位置由 Divider 在该次 measure/place 中分配。** `BoxLayout` 只提供 child-first measure（`layout_engine.cpp:559-581`），它**不执行父 intrinsic 回调**，也只会把所有子节点放在内容区左上角（`:893-903`），所以「用 Box 拿到尺寸」不等于完成 label 与 rails 的分配。决定：Divider 使用一个内部布局模型，在 measure 阶段同步测量标签、在 place 阶段设置标签 bounds 并产出 rails 几何；或者改用支持 `flex_grow`/`align_self` 的 Flex 子树并显式说明比例轨道与超宽收缩的算法（`layout_engine.cpp:632-636,734-789,975-1008` 确认 Flex 具备这些能力，不是「引擎不支持」）。
- **不得只改 Node bounds。** `WindowComponentServices` 的父辅助几何同步发生在 Text glyph 同步之后（`window_component_services.cpp:188-200`），在该阶段只改标签 Node 的 bounds 会让当帧字形仍留在旧位置。位置更新必须发生在 Text 发布之前，或在同帧显式重同步受影响 Text 的 placement。
- **垂直分割线的标签必须显式定义。** `LeafLayout` 既不测量也不放置子节点（`layout_engine.cpp:525-558,891-892`），因此垂直带标签不能沿用有标签子树。决定：垂直分割线忽略标签并在规格中写明（而不是静默丢弃一个已声明的 prop）。
- **空标签的几何要写清楚**：零占位、块级间距取「无标签」分支、整体高度按无线条规则计算。
- 保留「同轮测量、不读上一帧缓存」的目标，但它**不是 Box 的自动能力**，必须由上一条的模型与阶段落点保证。
- **`orientationMargin` 必须只剩一套规则。** 决策 14 规定「left/right 且 prop 未设置时即归零」，Divider 规格与任务则规定「先使用有效 Theme 比例，两级都无比例才归零」。默认 Theme 比例是 0.05，两者对同一个未设置 prop 的 left Divider 给出**不同的轨道宽度与 label padding**。决定统一为规格那一套：**组件显式比例覆盖 Theme 默认比例（0.05）；`Theme` 使用默认比例，显式 0 比例保留文字 padding，只有显式取 `None` 时才归零该侧轨道并把 `text_padding_inline` 换成 `sizePaddingEdgeHorizontal`（0）**。`Theme`/`None`/显式比例三态必须分别编码，「0」与「未设置」不得混用；决策 14、本节与规格、任务同步修正。
- **`plain` 的几何承诺要放宽。** 规格要求 `plain` 改文字字号/字重却承诺「线条几何保持不变」，但字号改变会改变标签宽度、整体高度与两条轨道长度。改为：`plain` 不改**线色、线宽与轨道规则**，允许文字度量引起布局重算。

## Token 引用清单

实现阶段 MUST 只引用下表 identity。除 `seed.lineWidth`（catalog 已锁定为 runtime、默认 1）外，其余非 seed Token 在 `catalog.yaml` 中均为 `metadata` 且 `upstream_default: null`，数值来源与验证边界见上文「上游数值来源与验证边界」。

| 用途 | Token identity | 默认/派生 |
| --- | --- | --- |
| 标题字号 h1–h5 | `ant.map.fontSizeHeading1`–`5` | 38 / 30 / 24 / 20 / 16 |
| 标题行高 h1–h5 | `ant.map.lineHeightHeading1`–`5` | 1.4 / 1.35 / 1.3 / 1.25 / 1.2（比值，按字号解算） |
| 标题字重与颜色 | `ant.alias.fontWeightStrong`、`ant.alias.colorTextHeading` | 600、`colorText` 同源 |
| 标题上下间距 | `ant.component.Typography.titleMarginTop`、`ant.component.Typography.titleMarginBottom` | `1.2em`、`0.5em`（相对标题字号） |
| 正文与次要文字 | `ant.map.colorText`、`ant.alias.colorTextDescription` | 主文字、`colorTextTertiary` |
| 语义文字 | `ant.map.colorSuccessText`、`ant.map.colorWarningText`、`ant.map.colorErrorText`、`ant.map.colorErrorTextHover`、`ant.map.colorErrorTextActive` | 暗色侧相对 step 3 / 2 / 4，见决策 9 |
| 禁用与链接 | `ant.alias.colorTextDisabled`、`ant.map.colorLink` | `colorTextQuaternary`、`seed.colorLink ?? seed.colorInfo` 的 key 6 |
| 分割线 | `ant.alias.colorSplit` | `getAlphaColor(colorBorderSecondary, colorBgContainer)`，线宽取 `seed.lineWidth` |
| 间距 | `ant.alias.marginLG`、`ant.alias.margin`、`ant.alias.marginXS` | `sizeLG` / `size` / `sizeXS` = 24 / 16 / 8（`sizeUnit` 4 派生）；Compact 下随之收缩，不得硬编码 |
| Divider 文字间距与朝向 | `ant.component.Divider.textPaddingInline`、`ant.component.Divider.orientationMargin`、`ant.component.Divider.verticalMarginInline` | `1em`、0.05、`marginXS` |
| Divider 带文字排版 | `ant.map.fontSizeLG`、`ant.alias.colorTextHeading`、`ant.alias.fontWeightStrong` | 带文字用 `fontSizeLG`／heading 色／500；`plain` 回落 `fontSize`／`colorText`／常规字重 |
| 等宽字族 | `seed.fontFamilyCode`（`SystemFontFamily::ui_monospace`） | 目前未被字体链消费，本 change 接通 |
| 行内 code / keyboard | 无对应上游 Component Token | RynUI 私有 typed adaptation：code 85%／`0.4em`／`0.2em 0.1em`、keyboard 90%／`0.4em`／`0.15em 0.1em` 与 2 倍下边框 |
| 高亮底色 | `mark` 上游硬编码 `gold[2]` | 以 `colors.mark_background` 承载固定参考色，不引入上游硬编码 |

## Risks / Trade-offs

### 实施进度与设计状态（截至 2026-10-01）

已完成：图标资源（2.1）、Theme Token 基线（2.2-2.6）、字体前置工作（3.1-3.3）、Typography 公开 API 与五级标题/语义色（4.1）、strong/italic/code/keyboard 的形状接入（4.2）。

**二次修订说明。** 决策 18-23 的第一版由一个独立审查（gpt-6.1-sol / xhigh）逐条对照源码核查，结论是六项都无法按原文落地，其中决策 23 要求的重挂载路径被 ComponentHost 明确禁止、同时违反 Divider 规格的 identity 要求。该审查还发现两处已提交实现中的真实缺陷（装饰度量的单位换算、inline token 的订阅缺口），两者已修复并补上可证伪的回归测试（提交 c2fa37f）。决策 18-23 与决策 14 已据此重写；下表反映修订后的状态。

| 剩余任务 | 决策 | 落地要点 |
| --- | --- | --- |
| 4.3 装饰渲染 | 18 | 三个独立片段 + 窗口级共享 quad store + 服务支持可变 range 与重映射；颜色与度量拆成不同 identity |
| 4.4 ellipsis | 19 | 截断在 TextState；需要 rows/suffix/expanded/reserved_inline 配置协议、保留全文与自然塑形、后缀缺字退化 |
| 4.5 copyable | 20 | 新增独立 ind_clipboard；可用性查询看写能力而非 has_text；晚绑定显式遍历 participant |
| 4.6 editable | 21 | 常驻双分支 + 内部 active/suspended 统一关闭交互与绘制 + 继承行高派生 control_height/viewport |
| 4.7 Link | 22 | build context 内 slot composition；Link 自己的 fragment 承载 interaction |
| Section 5 Divider | 23 + 14 | 标签节点常驻、模型可切换、禁止重挂载；标签尺寸由引擎测、轨道与放置由 Divider 在正确阶段分配；垂直忽略标签并写入规格 |
| Section 6 Gallery | 17 | 支持状态只在能力真实接通后提升 |
| Section 7 Windows 验收 | 4/5/14 的平台边界 | 必须在 4.x/5.x/6.x 全部落地后进行，否则证据无效 |

仍需在实现时定稿的**细节**（不改变上述方向）：省略的退化场景表与计数器精确数值、装饰 quad 的逐行几何取整、Divider 内部布局模型的 measure/place 阶段落点、焦点事务队列的刷新时机。这些应在对应任务的测试里固定。

**4.3b 装饰渲染的实现记录（未完成，勿重走弯路）。** 4.3a（服务层可变数量内容范围）已完成并提交。4.3b 曾实现到「片段注册 + 逐行几何 + quad 发布」，但测试失败后已整体回滚；以下是这次调查确认的事实：

- **四个片段的排序机制成立。** 实际遍历结果为「背景（before_children）→ glyph（before_children）→ 线（after_children）」，与决策 18 的预期一致；`register_scene_fragment` 允许一个组件注册多个片段。
- **em 度量正确。** 测试字体读出 `underline_position = -0.125`、`underline_thickness = 0.05`、`strikeout_position = 0.322`、`strikeout_thickness = 0.05`（全部 em 相对），坐标与决策 4 一致；装饰线位置为 `baseline - position * font_size`。
- **两个已修的真实缺陷（回滚时一并丢弃，重做时要注意）**：
  1. `ComponentHost` 只在**挂载期间**接受片段注册，而被连接的 props 在挂载**末尾**才写入 `state.typography`。因此装饰意图必须用 `read_prop` 直接从 props 读取，不能在挂载早期读 `state.typography`；副作用是「初始为 false、之后变为 true」的装饰无法再获得片段，这一点需要作为限制写进规格或改为常驻注册。
  2. 装饰帧无法监听 `composer_`/`surfaces_` 帧事件（`ComponentSceneComposer` 没有挂载钩子），因此片段必须在挂载前就绪，服务必须在 mount 之前 attach。
- **未定位的失败**：测试报出 `line_quads` 为空（`lnQ=0`），而同一组件的 `line_fragment` 已注册、`state.typography` 的 `underline`/`strikethrough` 读回为 `false`，尽管 props 读回为 `true`（`[init] ulProp=1 ulVal=1`）。**下次应从「`connect_prop` 的静态值回调是否真的把语义写进 `state.typography`」入手**，而不是继续在几何或发布路径上找。
- **测试夹具的一个真实陷阱**：`TextComponentHost` 必须在 `ComponentSceneComposer` 与 `RetainedSurfaceService` **之前**析构，否则宿主的 `dispose()` 会通过已销毁的服务清理组件并崩溃。夹具若把 host 声明在服务之前，必须显式在析构函数里先 `host->dispose()`。这次的 segfault 就是这个原因，与产品代码无关。
- **不要把 `paint_traversal()` 的 span 存进局部变量后跨其他宿主查询复用**：注册或移除片段会让宿主重建底层 vector，span 随之失效；需要先快照成 `std::vector`，并且**只调用一次** `paint_traversal()`（用同一次调用的 `begin()`/`end()`）。

**一致性问题状态：**

1. ~~`Title` 改 level 未满足规格~~ → **已修复**（提交 92fb975）。`Title::level` 改为响应式 `Prop<TypographyLevel>`，挂载后改级别会重解析标题 token 并保持组件 identity；测试断言组件数、`ComponentId` 与 `NodeId` 均不变、兄弟组件不被重塑。
2. ~~省略退化场景表缺失~~ → **已修复**。决策 7 与 Typography 规格原先给出不同输出（前者「`rows == 0` 只留后缀」，后者「保留最小可见内容」）。已按决策 19 的结论统一为一张固定表并写入规格的 `Degenerate ellipsis inputs` 场景：`rows == 0` 或可用宽度 `<= 0` 不产生行盒；行数超限但末行放不下后缀时**不加后缀**（后缀不作为可裁内容）；末行放不下任何完整字素时产生空行盒；显式换行保留为行结构，截断只作用于行数。决策 7 的相应表述同步以此表为准。
3. ~~strict 证据表述不一致~~ → **已修复**。实测：本 change `openspec validate --strict` 通过；全仓 `--all --strict` 为 28 passed / 6 failed，失败项 013/015/016/017/018/021 均为本 change 之前既存。`tasks.md` 1.2 已改为记录实测结果，不再声称全仓通过。当前 CLI 1.4.1 无 `doctor` 子命令，`AGENTS.md` 最低验证中的该步骤不适用。

## Validation

平台通用部分在 Windows `windows-msvc` Debug 上**执行一次**并记录 preset：公开 API/头文件隔离合同、标题级别度量与 Token 快照（含精确保留值与新增语义色 RGBA）、装饰命令顺序与几何、等宽字族解析与缺字回退（注入字体，不依赖系统）、省略（单行/rows/expandable/宽度重算与 `shape_count` 合同）、复制（含无 Input host 的纯 Typography 窗口、缺端口与平台失败）、编辑（提交/取消/maxLength/disabled/销毁清理与字体继承）、Link 键盘与指针激活、Divider 几何与各形式、主题颜色更新的最小失效与 idle，以及既有 Button/Input/Search/Selection 回归。

Windows 实机部分在 MSVC x64 + D3D12/DXIL 真实窗口运行 `rynui_token_gallery`：检查标题层级、行内语义（含等宽字形与真实 strong/italic face）、省略展开、复制成功反馈、原地编辑、Link 激活、Divider 各形式在亮色/暗色与系统 display scale 加 acceptance scale 下的可读性与裁切，并记录 driver、shader format、字体、scale、截图、诊断计数与退出码。

Linux 专属项只在真实 Linux 机器上完成，本 change 不声明 Linux 结果；Windows 的 DirectWrite 系统字体解析与 Linux 的 Fontconfig 匹配各自独立，不得互相代替。平台通用 CTest 只在一个平台跑一次，Windows/Linux 项只跑各自 toolchain/OS/input/font/GPU/evidence 相关测试。

## Migration Plan

依次提交规划修订、字体与主题基线（含前置工作）、平台通用 Typography、平台通用 Divider、Gallery 接入、Windows 实机与集成收口。每个阶段运行该阶段列出的测试后再以英文 conventional commit 提交。现有 `ryn::Text` 公开行为不变，新增 API 不需要迁移；决策 5 会改变 `ThemeFontResolver` 的内部签名，属仓库内部 API，需要同步更新既有测试夹具。

当前 OpenSpec CLI 拒绝以数字开头的项目约定 change 名，因此规划文件按既有 schema 手工创建，并使用支持该名称的 `openspec validate`；全仓已有 6 个与 034 无关的既有 strict 失败项，本 change 只要求自身 strict 通过。
