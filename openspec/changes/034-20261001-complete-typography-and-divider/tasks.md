# Tasks

平台通用（unit/headless/contract/benchmark/文档）任务 SHALL 只在 Windows `windows-msvc` Debug 上执行一次并记录 preset；只有依赖 OS、toolchain、window system、GPU/driver、system font、input/DPI 或 packaging 的行为才拆成 Windows 与 Linux 独立任务。平台通用 CTest 不在两个平台重复要求。上游数值的验证边界见 `design.md`「上游数值来源与验证边界」。

## 1. 规划与基线核对（平台通用）

- [x] 1.1 核对 `design-tokens/ant-design/6.6.5` 中 Typography/Divider Component Token、`fontSizeHeading1`–`5`、`lineHeightHeading1`–`5`、`fontWeightStrong`、`colorTextHeading`、`colorTextDescription`、`colorSuccessText`、`colorWarningText`、`colorErrorText(Hover/Active)`、`colorTextDisabled`、`colorSplit`、`colorLink`、`marginLG/margin/marginXS` 的 identity 与默认值，并确认 `SystemFontFamily::ui_monospace`、`font_family_code` 现状；通过 `openspec validate` 与 `git diff --check` 后以 `docs:` 提交规划产物
- [x] 1.2 落实规划评审结论：修正 palette key 与相对 step 的映射、按 dirty domain 重新划分 Typography/Divider Token 分组、把字体装饰度量·weight/slant face 解析·独立 clipboard 绑定·编辑子树与焦点事务列为显式前置任务、冻结 Divider 垂直高度与 `orientationMargin`/`plain` 语义并移除 `disabled`、明确平台通用与分平台 CTest 边界；本 change `openspec validate --strict` 与 `git diff --check` 通过后以 `docs:` 提交（实测记录：本 change 通过；全仓 `openspec validate --all --strict` 为 28 passed / 6 failed，失败项 013/015/016/017/018/021 均为本 change 之前既存，本 change 不负责修复，也不得据此声称「全仓 strict 通过」；当前 CLI 1.4.1 无 `doctor` 子命令，因此该步骤不适用）

## 2. 图标与主题 Token 基线（平台通用）

- [x] 2.1 锁定 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 五张官方 SVG（`@ant-design/icons-svg` 4.6.0，锁定 commit 与许可证 SHA256），扩展 `tools/generate_icon_assets.py` 图标表，重新生成 `src/icons/ant_design_icon_font.inc` 与 manifest，并把 `tools/verify_icon_assets.py` 的图标数量合同从 9 调整为 14；`tools/generate_icon_assets.py --check` 与 `tools/verify_icon_assets.py` 通过后以 `feat:` 提交
- [x] 2.2 扩展公开 Theme：`ThemeMapToken` 增加 `color_success_text`/`color_warning_text`/`color_error_text`/`color_link`（含 hover/active），`ThemeAliasToken` 增加 `color_split`；把 `derive_input_theme` 内的 alpha 搜索提取为 alias 与 Input 阴影共用的 `get_alpha_color` helper，并证明三种 outline 与 Dark/custom seed 的精确 RGBA 不变
- [x] 2.3 按 `design.md` 决策 10 的字段组新增 `TypographyThemeToken`（五级标题字号/行高、`title_margin_top_em`/`title_margin_bottom_em`、字族与 code 字族、常规与强调字重、`base_font_size`/`base_line_height`、`colors.*`、`code.*`、`keyboard.*`）与 `DividerThemeToken`（`colors.*`、`metrics.*`、`typography.*`），并提供逐级标题与各组字段的 typed override；override 中 em 比例与 fixed 长度不混用（标题间距、code/kbd 度量用比值，边框与圆角用 fixed 长度）
- [x] 2.4 接线 `theme_runtime`：`TokenIdentity` 按新分组拆为 `typography_colors`/`typography_headings`/`typography_fonts`/`typography_base_typography`/`typography_metrics`/`typography_inline_code`/`typography_inline_keyboard` 与 `divider_colors`/`divider_metrics`/`divider_typography` 并补 `alias_color_split`；逐项同步 `token_identity_name`（位置数组逐枚举核对，测试断言关键项名称而非只满足数量断言）、`collect_changed`（按字段组比较，一个已变化 identity 只 append 一次）、`dirty_phase_for`（颜色精确 `paint_material`，字体含 `text`，度量含 `measure_layout`，Divider 度量含 `hit_test`）与 `ThemeScope` 的整组及按域 accessor（含独立 `code_font_family()`）
- [x] 2.5 把 Typography/Divider 接入 `resolve_theme` 的值生命周期：derive、`apply_*_override`、组件 algorithm 隔离、父级继承、显式 override 优先、非法值在发布前失败；同步 `ThemeSnapshot` 构造声明与定义、两个 accessor、`operator==`、`snapshot_identity` hash 与 `serialize_snapshot`，并重新生成五份 golden（`default`/`dark`/`compact`/`dark-compact`/`compact-dark`），既有字段旧值保持不变以捕捉旧 palette/step 回归
- [x] 2.6 补齐 Token 合同测试：语义文字色默认 seed 的精确 RGBA（success/warning/error 与 link 三态）、灰度与边界 seed（灰度分支、纯黑、饱和红）、Dark 侧配方、Compact 不硬编码间距、既有 step 1–7 取值不变、每个新 identity 的名称与精确 dirty phase、typed capture 与等值更新不通知、override 与继承隔离；以 `feat:` 提交本阶段

## 3. 字体前置工作（平台通用）

- [x] 3.1 让默认字体链按 `SystemFontFamily` 参数化：`DefaultFontChainRequest` 增加 `preferred_monospace_fonts`，`DefaultFontChainResult` 增加独立的 `monospace_faces` 与 `monospace_identities()`（等宽族在前、UI 链在后，保证未覆盖码位仍可读）；`make_default_ui_font_resolver` 按字族分别缓存并按像素尺寸重载；Windows 解析 Cascadia Mono/Consolas/Lucida Console，Linux 走 Fontconfig 的 `monospace` 别名；用注入字体覆盖两族解析、缺字回退、族间缓存隔离、DPI 变化与等宽族不进入 UI 链，以 `feat:` 提交
- [x] 3.2 扩展 `font::FontMetrics` 的装饰字段（`underline_position`、`underline_thickness`、`strikeout_position`、`strikeout_thickness`）：underline 读 `FT_FaceRec`，strikeout 读 `FT_Get_Sfnt_Table(face, FT_SFNT_OS2)` 的 OS/2 表；四个字段统一保存为 **em 相对值**（freeType 的 underline 值按像素尺寸量化，14px 下步长约 1.25px，按像素归一化会在不同 DPI 间不稳定）；测试断言符号约定、厚度为正、跨光栅密度的稳定性与更大字号不出现负值，并同步修正 `design.md` 中「FreeType 直接提供四个字段」的错误表述，以 `feat:` 提交
- [x] 3.3 让 `ThemeFontResolver` 与 `runtime::SemanticTypography` 携带 `italic`，平台解析统一为 `platform_styled_descriptor(family, weight, italic)`，解析器惰性缓存键为 `(font_family, font_weight, italic, pixel_size)`，styled face 只 FRONT 在常规链之前、等宽请求仍追加 UI 链；Windows 枚举 family 字体列表按 weight/style 距离选最接近的 face（`GetFirstMatchingFont` 对可变字体会对所有 weight 返回同一文件），Linux 通过 Fontconfig 的 `FC_WEIGHT`/`FC_SLANT` 匹配并回传真实 style；取不到时回退常规 face 并写入 `diagnostic_fallbacks`；同步更新全部既有夹具与示例的 resolver 签名，并断言常规/强调/斜体的覆盖、族间缓存隔离与回退说明的精确性，以 `feat:` 提交

## 4. 平台通用 Typography（平台通用）

- [x] 4.1 发布 `include/ryn/typography.hpp` 与 `ryn::Title`（level 1–5）、`ryn::Text`、`ryn::Paragraph` 的 typed Props、typed slot 与 reactive `Prop<T>`，从 `rynui.hpp` 聚合头导出并保持现有 `ryn::Text(String)` 重载源码兼容；语义组件复用 `TextComponentHost`（字体链解析、场景生命周期、主题订阅与失效不重复实现），`Title`/`Paragraph` 只用 `level`、正文与段落共用 base 排版；`Title::level` 为响应式 `Prop<TypographyLevel>`，挂载后改级别保持组件 identity（node、component id 与组件数不变）；语义颜色统一走 Typography Component Token 组（含 secondary/disabled），`TextTone` 不再劫持语义色；测试覆盖五级标题的字号链与行为度量、六种语义色、emphasis 触发自身 reshape 且不重挂载、reactive 内容/type/level 的局部失效与 identity 保持，以及主题切换下标题按 Component Token 重算字号且不重挂载、不影响兄弟组件；public API 合同、标题级别度量、`type`/`disabled` 颜色、`Prop<T>` 响应式更新与局部失效测试通过后以 `feat:` 提交
- [x] 4.2 实现 `strong`/`italic`/`code`/`keyboard` 的形状接入：`TypographySemantics` 增加 `code`/`keyboard`/`mark`，`resolve_semantic_typography` 在 `code`/`keyboard` 时切换到 `font_family_code` 并按 `InlineCodeThemeToken::font_scale` 缩放字号，`strong` 请求强调字重、`italic` 请求斜体 face；夹具记录每次解析请求，断言 strong 请求强调字重、italic 请求斜体、code/keyboard 请求等宽族且分别在各自 inline token 字号解析、plain 仍为常规字重 UI 族；以 `feat:` 提交（`code`/`keyboard` 的底色/边框方块与 `underline`/`delete`/`mark` 的装饰渲染归入 4.3）
- [x] 4.3a 按 `design.md` 决策 18 的落地要点扩展 `RetainedSurfaceService`，解开装饰的前置阻塞：新增内容范围 API（`create_content_range`/`set_content_range`/`update_content_range`），由调用方持有 fragment、支持**可变数量**的 quad（经 `QuadInstanceStore::replace` 在数量变化时重分配并计入 `fragment_remaps`），内容范围不设 surface 的 16 层上限（改用独立上限），并把有限性校验提取为共用函数；`update_surface` 保持「数量不变」的既有契约与**纯栈缓冲**（该路径受交互帧的分配计数断言约束，改用堆容器会破坏 `input_pointer`/`input_keyboard`/`input_scene_allocation`）；平台通用回归通过后以 `feat:` 提交（装饰的分层接线与几何断言归入 4.3b）
- [x] 4.3b 实现 `mark`、`code`、`keyboard` 背景在字形**之前**绘制，`underline`/`delete` 装饰线在字形**之后**绘制且位置厚度取自字体装饰度量（em 相对，向下取 `baseline - position * font_size`）；覆盖单行、多行段落、同名叠加、滚动 translation、裁剪与 DPI 变化，用命令顺序断言加几何断言证明层级与对齐，并证明装饰颜色变化只更新材质（颜色与度量必须分离为不同 TokenIdentity）后以 `feat:` 提交
- [x] 4.4 实现 `ellipsis`：单行以自然宽度与可用宽度比较判定截断（不依赖换行 overflow）、多行用 `lines.size() > rows` 且末行留足后缀与操作入口宽度、`expandable` 展开/收起入口；新增不污染 retained scene 的候选测量通道并给出 `shape_count`/`measure_count` 合同；覆盖字素边界、退化输入（`rows == 0`、宽度小于后缀、显式换行）、展开/收起 identity、宽度重算可重复性与只触发自身 Measure/Layout 后以 `feat:` 提交
- [x] 4.5 实现内容模型与 `copyable`：区分原始全文与显示文本，复制与编辑取值使用原始全文；在 `WindowComponentServices` 增加独立于文本输入的剪贴板绑定入口、可空访问器与晚绑定参与者通知钩子；覆盖成功、端口未绑定、平台失败、晚绑定后可用、窗口失焦、组件销毁与主题切换后的状态清理，并包含一个**没有任何 Input 组件或 host** 的纯 Typography 窗口用例；以 `feat:` 提交
- [x] 4.6 实现 `editable`：首次挂载预建编辑子树（真实 `ryn::Input`，受布局与可见性控制），编辑态只切换布局/可见性/eligibility 与焦点；补齐 Input 的内部 `SemanticTypography` 继承入口与 blur/cancel 生命周期回调，冻结 Esc 对 IME 组合与编辑草稿的优先级，并新增「dispatch 之后执行、校验 generation」的焦点事务；覆盖提交/取消、受控回写与外部拒绝回写、编辑中继承标题字体、maxLength、disabled、键盘激活后焦点转移与销毁清理后以 `feat:` 提交
- [x] 4.7 实现 `ryn::Link` 的链接语义颜色与完整交互：指针、Tab、键盘激活、悬浮/按下/键盘焦点呈现、`disabled` 不可命中与不可聚焦并清理捕获与焦点；确认 Tab 顺序与命中顺序不回归既有控件后以 `feat:` 提交

## 5. 平台通用 Divider（平台通用）

- [x] 5.1 发布 `include/ryn/divider.hpp` 与 `ryn::Divider`，实现水平/垂直、`orientation`（left/right/center）、`orientationMargin`、`dashed`、`plain` 与可选 typed 文字 slot 的几何与材质：水平无线条使用 `marginLG` 上下间距，水平带文字使用 `margin`、`colorTextHeading`、字重 500 与 `fontSizeLG`；`orientationMargin` 区分主题默认值与组件显式覆盖，仅当 left/right 且两级都无有效比例时才把对应轨道归零并使用 `sizePaddingEdgeHorizontal`；垂直以当前行高为基准的固定高度加相对偏移；`plain` 只改文字；文字超宽时退化不溢出；不提供 `disabled` 或任何交互状态；`DividerComponentHost` 挂载/销毁/几何同步与 `LayoutStyle` 边界、各形式几何、方向与外边距、`Prop<T>` 响应式更新和颜色变化只更新材质测试通过后以 `feat:` 提交

## 6. Gallery 与参考数据（平台通用）

- [ ] 6.1 在 Token Gallery 接入 Typography 与 Divider 真实样例（五级标题、行内语义、省略展开、复制与编辑、Link、水平/带文字/垂直/虚线/plain 分割线），同步 `gallery/ant-design/6.6.5/support-overlay.json` 的 `status`、`supported_scope`、`missing_scope`、`evidence_identifiers` 与 reference catalog 合同；`missing_scope` 如实保留 `tooltip` 浮层等未覆盖项；只有在能力真实接通后才提升 support，不提前提升 hover/active/Link，也不把私有 code/kbd 字段冒充新增上游 Component Token；Gallery 参考目录合同测试、文档合同测试与样例帧测试通过后以 `feat:` 提交

## 7. Windows 验收（Windows）

- [ ] 7.1 使用 `windows-msvc` preset clean configure 完成 Debug/Release build，并在 MSVC x64 + D3D12/DXIL 真实窗口运行 `rynui_token_gallery`：核对五级标题、真字重与斜体 face、行内语义（含 `code`/`keyboard` 等宽字形与缺字回退）、省略单行/多行/展开、无 Input 窗口的复制成功反馈、原地编辑提交与取消及字体继承、Link 指针与键盘激活、Divider 各形式；在系统 display scale 与 acceptance render scale 1.0/1.25/1.5/2.0 下检查布局、裁切与装饰对齐，保存截图、driver、shader format、字体、scale、诊断计数与退出码到 `evidence/windows-*.md`
- [ ] 7.2 只运行 Windows 平台分支相关的受影响测试（DirectWrite 字体 face 解析、图标资源验证、shader/lock/license、未跟踪依赖）与 Windows evidence passed contract、`git diff --check`；不重复已经通过的平台通用 CTest，以英文 `test: validate Windows typography and divider` 提交 Windows 证据；不修改 Linux 条目，不主动 push

## 8. 平台通用集成验收（平台通用）

- [ ] 8.1 在 Windows `windows-msvc` Debug 上运行**一次**完整 CTest、本 change strict、全仓 strict、可用 doctor 与 `git diff --check`；记录未通过项与平台边界，补齐平台通用证据并更新 `README.md` 当前进展表（避免把未验收的平台结果描述为通过），满足门槛后以 `test:` 提交

## 9. Linux 验收（Linux）

- [ ] 9.1 在真实 Linux 的正式 `linux-gcc`/`linux-clang` preset 完成受影响构建与平台分支 CTest（Fontconfig weight/slant 匹配、`ui_monospace` 系统等宽字族解析、Vulkan/SPIR-V、Ninja Multi-Config），独立记录 Linux 构建证据
- [ ] 9.2 在原生 Wayland 真实窗口以至少两档实际 display scale 检查标题层级、真字重与斜体、`code`/`keyboard` 等宽字形与回退、装饰对齐、省略展开、复制与编辑、Link 激活及 Divider 各形式，保存截图、compositor、driver、font、scale、诊断与退出码；不以 XWayland、WSLg 或 Windows 结果代替
- [ ] 9.3 运行 Linux 受影响测试与 evidence contract，以英文提交 Linux 证据文件，不修改 Windows 清单

## 10. 收口（准备 archive 时执行）

- [ ] 10.1 在准备 archive 时核对平台通用、Windows 与 Linux 各自 checkbox 与 evidence 是否真实完成，运行最终 strict validate、`git diff --check`、remote SHA 与 clean worktree 检查，并只复测本次新增改动实际影响的测试项；确认 README、AGENTS、architecture、generated Token 文档与 OpenSpec 职责未混写；本项不替代任何平台验收，也不自动 archive 或 push
