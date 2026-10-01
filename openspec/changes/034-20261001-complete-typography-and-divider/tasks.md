# Tasks

## 1. 规划与基线核对

- [x] 1.1 核对 `design-tokens/ant-design/6.6.5` 中 Typography/Divider Component Token、`fontSizeHeading1`–`5`、`lineHeightHeading1`–`5`、`fontWeightStrong`、`colorTextHeading`、`colorTextDescription`、`colorSuccessText`、`colorWarningText`、`colorErrorText(Hover/Active)`、`colorTextDisabled`、`colorSplit`、`colorLink`、`marginLG/margin/marginXS` 的 identity 与默认值，并确认 `SystemFontFamily::ui_monospace`、`font_family_code` 现状；把核对结论与后续实现要引用的 Token identity 追加到 `design.md`，确认本 change strict、全仓 strict 与 `git diff --check` 结果后以 `docs:` 提交规划产物

## 2. 图标与主题 Token 基线（平台通用）

- [ ] 2.1 锁定 `CopyOutlined`、`CheckOutlined`、`EditOutlined`、`DownOutlined`、`UpOutlined` 五张官方 SVG（`@ant-design/icons-svg` 4.6.0，锁定 commit 与许可证 SHA256），扩展 `tools/generate_icon_assets.py` 图标表，重新生成 `src/icons/ant_design_icon_font.inc` 与 manifest，并把 `tools/verify_icon_assets.py` 的图标数量合同从 9 调整为 14；`tools/generate_icon_assets.py --check` 与 `tools/verify_icon_assets.py` 通过后以 `feat:` 提交
- [ ] 2.2 扩展公开 Theme：`ThemeMapToken` 增加 `color_success_text`/`color_warning_text`/`color_error_text`，`ThemeAliasToken` 增加 `color_split`，新增 `TypographyThemeToken`（五级标题字号/行高、标题上下间距、`font_weight_strong`、`font_family_code`、`text_color`、`text_description`、`success/warning/error/disabled` 颜色、`code`/`keyboard` 度量与 `mark` 高亮色）与 `DividerThemeToken`（线宽、`text_padding_inline`、`orientation_margin`、`vertical_margin_inline`、水平/带文字外边距、带文字字号与字重、`color_split`、`color_text`）；补齐 `TypographyThemeConfig`/`DividerThemeConfig` override、`theme_runtime::TokenIdentity` 条目、`token_identity_name`、`dirty_phase_for` 与 changed-identity 比较；Token 快照、算法链、等值与 override 测试通过后以 `feat:` 提交

## 3. 平台通用 Typography 基础（平台通用）

- [ ] 3.1 让默认字体链按 `SystemFontFamily` 参数化：`make_default_ui_font_resolver` 按字族与像素尺寸分别缓存，`ui_monospace` 增加各平台等宽首选族并复用现有缺字回退与验证字体兜底，`DefaultFontChainRequest` 增加等宽首选入口；字体链测试覆盖两族解析、缓存命中、DPI 变化与缺字回退后以 `feat:` 提交
- [ ] 3.2 发布 `include/ryn/typography.hpp` 与 `ryn::Title`（level 1–5）、`ryn::Text`、`ryn::Paragraph` 的 typed Props、typed slot 与 reactive `Prop<T>`，从 `rynui.hpp` 聚合头导出并保持现有 `ryn::Text(String)` 重载源码兼容；实现 `TypographyComponentHost` 的挂载、销毁、主题订阅与几何同步，使五级标题使用上游字号/行高/`fontWeightStrong`/`colorTextHeading` 与 `titleMarginTop`/`titleMarginBottom`；公开 API 合同、标题级别度量、`type`/`disabled` 颜色、`Prop<T>` 响应式更新与局部失效测试通过后以 `feat:` 提交
- [ ] 3.3 实现 `code`/`keyboard` 的等宽字族接入与外观（`code` 底色、内边距、边框、85% 字号与圆角；`keyboard` 底色、边框、2px 下边框与 90% 字号），以及 `strong`/`italic` 的字重与字族选择；等宽字形度量、缺字回退、`code`/`keyboard` 几何与主题响应测试通过后以 `feat:` 提交
- [ ] 3.4 实现 `underline`、`delete` 与 `mark` 的装饰渲染：下划线与删除线按逐行 `baseline`/`width` 与 run 字体度量绘制 quad overlay，`mark` 绘制背景高亮；覆盖单行、多行段落、滚动 translation、裁剪与 DPI 变化下的对齐，并证明装饰颜色变化只更新材质后以 `feat:` 提交
- [ ] 3.5 实现 `ellipsis`：单行截断、多行 `rows` 截断、`expandable` 展开/收起入口，按字素边界收缩并复用测量缓存，宽度或内容变化后重新计算；省略后缀与边界、`rows` 裁剪、展开/收起状态与 identity、宽度重算的可重复性、只触发自身 Measure/Layout 测试通过后以 `feat:` 提交
- [ ] 3.6 实现 `copyable`：复制入口、成功反馈图标与主题成功色、超时复原与 deadline 清理、自定义提示文案；在 `WindowComponentServices` 增加可空剪贴板访问器与参与者通知钩子，覆盖成功、端口未绑定、平台失败、窗口失焦、组件销毁与主题切换后的状态清理；剪贴板写入、反馈计时、失败可观察与零残留测试通过后以 `feat:` 提交
- [ ] 3.7 实现 `editable`：编辑态原地切换为单行输入并复用 `TextEditorStore`/`TextInputSessionHost` 与 `PressableBehavior`，支持回车/失焦提交、Esc 取消、`maxLength` 与 `disabled`，编辑态继承被编辑元素的 `SemanticTypography`；提交/取消回调、受控回写、maxLength、disabled、键盘与销毁清理测试通过后以 `feat:` 提交
- [ ] 3.8 实现 `ryn::Link` 的链接语义颜色与完整交互：指针、Tab、键盘激活、悬浮/按下/键盘焦点呈现、`disabled` 不可命中与不可聚焦，并确认 Tab 顺序与命中顺序不回归既有控件；Link 激活、禁用、焦点可见性与相邻控件回归测试通过后以 `feat:` 提交

## 4. 平台通用 Divider（平台通用）

- [ ] 4.1 发布 `include/ryn/divider.hpp` 与 `ryn::Divider`，实现水平/垂直、`orientation`（left/right/center）、`orientationMargin`、`dashed`、`plain` 与可选 typed 文字 slot 的几何与材质：水平无线条使用 `marginLG` 上下间距，水平带文字使用 `margin`、`colorTextHeading`、字重 500 与 `fontSizeLG`，轨道按 `orientationMargin` 比例分配并在未显式设置时为 left/right 使用 `sizePaddingEdgeHorizontal` 边距，垂直延续行高并保留 `top` 偏移与 `verticalMarginInline`，`plain` 回落 `colorText`/常规字重/`fontSize`；`DividerComponentHost` 挂载/销毁/几何同步与 `LayoutStyle` 边界、各形式几何、方向与外边距、`Prop<T>` 响应式更新和颜色变化只更新材质测试通过后以 `feat:` 提交

## 5. Gallery 与参考数据（平台通用）

- [ ] 5.1 在 Token Gallery 接入 Typography 与 Divider 真实样例（五级标题、行内语义、省略展开、复制与编辑、Link、水平/带文字/垂直/虚线/plain 分割线），同步 `gallery/ant-design/6.6.5/support-overlay.json` 的 `status`、`supported_scope`、`missing_scope`、`evidence_identifiers` 与 reference catalog 合同，`missing_scope` 如实保留 `tooltip` 浮层等未覆盖项；Gallery 参考目录合同测试、文档合同测试与样例帧测试通过后以 `feat:` 提交

## 6. Windows 验收（Windows）

- [ ] 6.1 使用 `windows-msvc` preset clean configure 完成 Debug/Release build，并在 MSVC x64 + D3D12/DXIL 真实窗口运行 `rynui_token_gallery`：核对五级标题、行内语义（含 `code`/`keyboard` 等宽字形）、省略单行/多行/展开、复制成功反馈、原地编辑提交与取消、Link 指针与键盘激活、Divider 各形式；在系统 display scale 与 acceptance render scale 1.0/1.25/1.5/2.0 下检查布局、裁切与装饰对齐，保存截图、driver、shader format、字体、scale、诊断计数与退出码到 `evidence/windows-*.md`
- [ ] 6.2 运行 Windows 受影响完整 CTest、Windows evidence passed contract、shader/lock/license 与图标资源验证、未跟踪依赖检查与 `git diff --check`，以英文 `test: validate Windows typography and divider` 提交 Windows 证据；不修改 Linux 条目，不主动 push

## 7. 平台通用集成验收（平台通用）

- [ ] 7.1 运行完整 CTest、本 change strict、全仓 strict、可用 doctor 与 `git diff --check`；记录未通过项与平台边界，补齐平台通用证据并更新 `README.md` 当前进展表（避免把未验收的平台结果描述为通过），满足门槛后以 `test:` 提交

## 8. Linux 后续验收（Linux）

- [ ] 8.1 后续在真实 Linux 的正式 `linux-gcc`/`linux-clang` preset 完成受影响构建与 CTest，核对 `ui_monospace` 系统等宽字族解析、Fontconfig 字体来源、Vulkan/SPIR-V 与 Ninja Multi-Config，独立记录 Linux 构建证据
- [ ] 8.2 后续在原生 Wayland 真实窗口以至少两档实际 display scale 检查标题层级、`code`/`keyboard` 等宽字形与回退、装饰对齐、省略展开、复制与编辑、Link 激活及 Divider 各形式，保存截图、compositor、driver、font、scale、诊断与退出码；不以 XWayland、WSLg 或 Windows 结果代替
- [ ] 8.3 运行 Linux 受影响测试与 evidence contract，以英文提交 Linux 证据文件，不修改 Windows 清单

## 9. 收口（准备 archive 时执行）

- [ ] 9.1 在准备 archive 时核对平台通用、Windows 与 Linux 各自 checkbox 与 evidence 是否真实完成，运行最终 strict validate、受影响 CTest、`git diff --check`、remote SHA 与 clean worktree 检查，确认 README、AGENTS、architecture、generated Token 文档与 OpenSpec 职责未混写；本项不替代任何平台验收，也不自动 archive 或 push
