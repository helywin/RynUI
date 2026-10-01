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

退化行为需要冻结并有测试：`rows == 0`、可用宽度小于后缀本身、文本含显式换行、`expandable` 与 `rows` 同时设置。

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
- **`orientation_margin` 语义**：区分两个来源。Theme `DividerThemeToken::metrics.orientation_margin` 是 Component Token 默认值（`0.05`）；组件 prop `orientationMargin` 是显式覆盖。仅当 orientation 为 `left`/`right` **且 prop 未设置**时才按上游 `no-default-orientation-margin` 规则把该侧轨道宽度归零、并把 `text_padding_inline` 换成 `sizePaddingEdgeHorizontal`（`0`）。spec 与 tasks 统一到这一条。
- **`plain` 语义**：只改**文字**（`plain_text` 色、常规字重、`plain_font_size`），不改线条填充。spec 中「更浅的填充色」删除。
- **`disabled`**：本 change **不提供** `Divider.disabled`。Divider 没有交互状态，proposal／spec／tasks 中相关表述一律删除。

#### 实施时核实的两个约束（影响 Divider 的测量方案）

1. **`LeafLayout` + intrinsic measure 不会先测量子节点。** `LayoutEngine::measure_node` 对 `LeafLayout` 直接调用 intrinsic 回调，只有 `BoxLayout`/`FlexLayout` 才先 `measure_node` 各子节点。因此若把标签作为分割线的子节点、同时给分割线挂 intrinsic measure，回调里**读不到标签的已测宽度**。
2. **标签宽度只能在文本同步后得知。** 文本宽度来自 `TextSceneService` 的 shape 结果，而布局测量发生在窗口同步之前，所以「测量时按标签实际宽度分配两条轨道」存在先有鸡还是先有蛋的问题；Button 那类组件用「缓存测量 + 版本修正」跨帧收敛解决。

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

### 18. 装饰分层通过每组件的前置/后置片段实现（4.3 定稿）

`ComponentHost::Record` 已经同时持有 `before_children_fragments` 与 `after_children_fragments` 两个列表，`register_scene_fragment` 也已接受 placement，所以「背景在 glyph 前、装饰线在 glyph 后」不需要新的分层机制，只需要让一个组件能注册**两个**片段。决定：

- 文本组件注册两个片段：`before_children`（`mark` 高亮底、`code`/`keyboard` 的底与边框）与 `after_children`（`underline`/`delete` 线）。glyph 片段保持现在的 `before_children`，并由注册顺序保证背景先于 glyph。
- 片段的 quad 命令由 `TextComponentHost` 在几何同步阶段写入：位置取自 `TextMeasurement` 的逐行 `baseline`/`width`/`content_bounds`，线位置与厚度取自该行 run 的 `font::FontMetrics` 装饰字段（决策 4），按 `font_size` 换算为逻辑像素；`mark` 仅覆盖 `content_bounds` 的水平范围。
- 纯 `Text(String)` 与 Icon 不注册任何装饰片段，保持零开销；只有声明了装饰/背景语义的组件才付出额外片段。
- 备选是把背景交给子节点、线留给父节点。否决理由：会造成一个语义组件对应多个组件记录，命中测试、主题订阅与销毁路径都要跟着复制，收益不足。

### 19. 省略在 `TextState` 的塑形时执行（4.4 定稿）

判据已在决策 7 冻结（自然宽度比较、`lines.size() > rows`、字素边界、退化行为）。候选测量的落点决定如下：

- 在 `TextState` 内部完成截断，而不是在组件层做二分搜索：`synchronize` 先按可用宽度塑形一次；若需要截断，则用 `TextBoundaryMap::grapheme_bytes` 枚举字素边界作为二分点，对每个候选按「候选文本 + 省略后缀」重新 `shape` + `measure` 直到收敛。组件层只读最终测量结果。
- 复用同一条 `font::FontRuntime` 塑形路径，不新增测量后端；候选塑形不写入 glyph 实例（只有最终文本进入 `GlyphScene`），因此不污染 retained scene。
- 可观察合同：`TextStateCounters` 增加 `ellipsis_searches` 与 `ellipsis_shapes`，测试据此断言「同宽度重复查询不新增塑形」以及二分搜索次数上界为 `log2(字素数) + 1`。
- 退化行为按决策 7 固定：`rows == 0` 时只留省略后缀（无后缀则留空）、可用宽度小于后缀时不加后缀、显式换行保留换行结构。

### 20. 剪贴板绑定入口由窗口 adapter 显式调用（4.5 定稿）

决策 8 已冻结「独立于文本输入的绑定 + 晚绑定通知」，但没定谁调用。决定：

- 调用方是**窗口 adapter**，即构造 `WindowComponentServices` 并驱动帧循环的那一层（Gallery 与各示例的 runtime）。它在启动时调用 `bind_text_edit(platform, clipboard)`，而不是等 Input 组件挂载时才碰巧绑定。
- `WindowComponentServices` 增加只读的剪贴板可用性查询，组件据此决定复制入口是否可用；`bind_text_edit` 完成时通知已挂载的参与者（`WindowComponentParticipant` 新增带默认实现的钩子），实现晚绑定后可用。
- 未绑定的窗口仍然完整渲染与排版，复制入口不可用且可观察，不抛出。测试同时覆盖「绑定的窗口」与「完全没有 Input 组件与 host 的窗口」两种。
- 明确不做：不让 `copyable` 依赖 `TextClipboardCommands`（那是为 editor owner 设计的），也不为排版组件创建 `TextEditorStore`。

### 21. `editable` 用预建子树 + 延迟焦点事务（4.6 定稿）

决策 12 已冻结「首次挂载预建编辑子树」。三处未定的接口落点如下：

- **字体继承**：`InputComponentHost` 增加内部入口，允许挂载方为一个输入提供一个 `Prop<runtime::SemanticTypography>`，与 Text 的 `semantic_typography` 走同一条通道；编辑态由排版组件把当前解析结果传进去，因此标题编辑不会掉回 14px。
- **编辑生命周期**：`InputComponentHost` 增加内部 `on_commit`/`on_cancel`/`on_blur` 回调注册（只暴露给仓库内部调用方，不进入公开 API）。Esc 优先级固定为：有 IME composition 时先取消 composition 并保持编辑态，再次 Esc 才放弃草稿。
- **焦点事务**：`FocusManager` 增加「请求队列 + 派发结束后刷新」的机制。组件 handler 在派发期间只入队请求，刷新在 `begin_operation` 作用域之外执行，并在应用前校验 `ComponentId` 的 generation，避免 handler 期间销毁导致悬空。这与 019 记录的「`request_focus` 在 dispatch 内被拒绝」是同一个根因，本 change 把它修在焦点管理器里，而不是在组件 handler 里绕过。
- 受控语义：编辑草稿优先于受控 `content`；提交后等待外部回写，外部拒绝回写时保留草稿并保持编辑态，状态可观察。该行为必须有测试。

### 22. Link 走独立输入参与者，不改 `TextComponentHost` 的职责（4.7 定稿）

`TextComponentHost` 现在完全不接触输入服务，给它加可选输入服务会把「文本渲染宿主」变成「什么都做的宿主」。决定：

- `Link` 是独立组件宿主 `LinkComponentHost`（`WindowComponentServices` 参与者），像 `SelectionComponentHost` 一样借用 `interactions`/`focus`/`pointer`，并复用 `TextComponentHost` 的文本渲染能力（通过其 `mount`/`resolve_fonts` 接口）绘制链接文字。
- 交互合同与 Button 一致：`InteractionRegistry` 的 eligible/focusable、`FocusManager` 的 Tab/Enter/Space、`PressableBehavior` 的指针手势与 capture；`disabled` 时取消 capture、清除 eligibility、清理焦点并更新 hit snapshot。
- 备选是给 `TextComponentHost` 增加可选输入服务。否决理由：那会让每个纯文本组件都持有永不使用的输入指针，并把焦点合同扩散到排版宿主。

### 23. 带标签分割线走路径 A：标签节点由布局引擎测量（Divider 定稿）

决策 14 记录的约束是：`LeafLayout` + intrinsic measure 不会先测量子节点，而标签宽度只能在文本同步后得知。定稿选择**路径 A**，并给出具体形态：

- **无标签**：`LeafLayout` + intrinsic measure，返回 `{可用宽度, 线宽 + 2 × 块级间距}`，渲染一条贯通全宽的轨道。
- **有标签**：改为 `BoxLayout`，由布局引擎先测量标签子节点，再组合整体高度；标签自身是一个 `Text` 子组件，它的固有测量回调走已有的 `TextSceneService::synchronize_measurement` 路径，与 Button 内容文本完全相同。轨道与标签的几何由父节点在一次几何同步里按测量结果分配，不读「上一帧缓存」。
- **标签内容变化**：因为布局模型在「有标签」与「无标签」之间切换，标签从空变非空（或反向）时以显式重挂载收口；仅标签文本内容变化时不需要重挂载，由文本组件的 `Prop<String>` 通路处理。
- **备选路径 B**（单节点缓存测量 + 版本修正）否决理由：首帧高度可能不准，且需要额外的版本账本；路径 A 把组合交给已经过验证的布局引擎，符合「能局部解决的不要自己重算」。
- 垂直分割线无论有无标签都用 `LeafLayout` + intrinsic measure，因为它不参与水平轨道分配。

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

### 实施进度与设计冻结状态（截至 2026-10-01）

已完成：图标资源（2.1）、Theme Token 基线（2.2-2.6）、字体前置工作（3.1-3.3）、Typography 公开 API 与五级标题/语义色（4.1）、strong/italic/code/keyboard 的形状接入（4.2）。

剩余 14 项原先各自卡在一个**未定的架构决策**上，这些决策现已全部冻结（决策 18-23），实施阶段不再需要重新选择：

| 剩余任务 | 已冻结的决策 | 落点 |
| --- | --- | --- |
| 4.3 装饰渲染 | 决策 18 | 每组件注册 before_children（背景）与 after_children（装饰线）两个片段 |
| 4.4 ellipsis | 决策 19 | 截断在 TextState::synchronize 内完成，TextStateCounters 增加 ellipsis_searches/ellipsis_shapes |
| 4.5 copyable | 决策 20 | 窗口 adapter 启动时显式 bind_text_edit，晚绑定通知参与者 |
| 4.6 editable | 决策 21 | 预建子树 + Input 内部 typography/blur/cancel 入口 + FocusManager 延迟焦点事务 |
| 4.7 Link | 决策 22 | 独立 LinkComponentHost 参与者，复用文本渲染而不改排版宿主职责 |
| Section 5 Divider | 决策 23 + 决策 14 | 有标签用 BoxLayout（引擎测量标签），无标签用 LeafLayout + intrinsic measure |
| Section 6 Gallery | 决策 17 | 支持状态只在能力真实接通后提升 |
| Section 7 Windows 验收 | 决策 4/5/14 的平台边界 | 必须在 4.x/5.x/6.x 全部落地后进行，否则证据无效 |

实施阶段仍需在实现时定稿的**细节**（不改变上述方向）：省略的二分收敛判据与计数器精确数值、装饰 quad 的逐行几何取整、BoxLayout 标签与轨道的分配顺序、以及焦点事务队列的刷新时机。这些属于实现细节，应在对应任务的测试里固定下来。

## Validation

平台通用部分在 Windows `windows-msvc` Debug 上**执行一次**并记录 preset：公开 API/头文件隔离合同、标题级别度量与 Token 快照（含精确保留值与新增语义色 RGBA）、装饰命令顺序与几何、等宽字族解析与缺字回退（注入字体，不依赖系统）、省略（单行/rows/expandable/宽度重算与 `shape_count` 合同）、复制（含无 Input host 的纯 Typography 窗口、缺端口与平台失败）、编辑（提交/取消/maxLength/disabled/销毁清理与字体继承）、Link 键盘与指针激活、Divider 几何与各形式、主题颜色更新的最小失效与 idle，以及既有 Button/Input/Search/Selection 回归。

Windows 实机部分在 MSVC x64 + D3D12/DXIL 真实窗口运行 `rynui_token_gallery`：检查标题层级、行内语义（含等宽字形与真实 strong/italic face）、省略展开、复制成功反馈、原地编辑、Link 激活、Divider 各形式在亮色/暗色与系统 display scale 加 acceptance scale 下的可读性与裁切，并记录 driver、shader format、字体、scale、截图、诊断计数与退出码。

Linux 专属项只在真实 Linux 机器上完成，本 change 不声明 Linux 结果；Windows 的 DirectWrite 系统字体解析与 Linux 的 Fontconfig 匹配各自独立，不得互相代替。平台通用 CTest 只在一个平台跑一次，Windows/Linux 项只跑各自 toolchain/OS/input/font/GPU/evidence 相关测试。

## Migration Plan

依次提交规划修订、字体与主题基线（含前置工作）、平台通用 Typography、平台通用 Divider、Gallery 接入、Windows 实机与集成收口。每个阶段运行该阶段列出的测试后再以英文 conventional commit 提交。现有 `ryn::Text` 公开行为不变，新增 API 不需要迁移；决策 5 会改变 `ThemeFontResolver` 的内部签名，属仓库内部 API，需要同步更新既有测试夹具。

当前 OpenSpec CLI 拒绝以数字开头的项目约定 change 名，因此规划文件按既有 schema 手工创建，并使用支持该名称的 `openspec validate`；全仓已有 6 个与 034 无关的既有 strict 失败项，本 change 只要求自身 strict 通过。
