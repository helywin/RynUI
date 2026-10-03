# Input 原生桌面 API

Input、Password、Search 使用共用 typed Props 和长期编辑器。单行编辑已支持受控/非受控值、Unicode scalar 硬 maxLength、grapheme 选择/移动、剪贴板、undo/redo 与平台组合输入。视觉来自 Theme；LayoutStyle 只控制外部布局。

## 焦点引用与系统输入提示

OTP 的组级候选模型按 grapheme 分格，去除 CR/LF，默认六格、容量 1–1024。单字符替换保留其他格，多字符粘贴保留前缀并替换尾部；外部值的暂不可见后缀在扩大容量时恢复，用户编辑按当前容量截断。formatter 在发布前收到以空格表示空洞的候选；异常、重入改写或退休组拒绝旧候选。authoritative 回写绕过 formatter，相同 controlled echo 保留格子空洞和本次事务。填满且不同的用户候选才标记完成。当前模型已实现，公开组件与窗口验收按 054 后续阶段推进。

```cpp
ryn::InputRef input;
ryn::Input(ryn::InputProps{}.ref(input)
               .autoFocus()
               .purpose(ryn::InputPurpose::Name)
               .capitalization(ryn::InputCapitalization::Words)
               .autocorrect(true)
               .onFocus([] {})
               .onBlur([] {}));
// 在挂载及首次布局之后使用。
input.focus({ryn::InputFocusCursor::All});
input.blur();
```

InputRef 可以复制共享同一个绑定，bound 在挂载前/卸载后为 false；focus/blur/select 无合法绑定时返回 false。一个引用不能同时绑定两个存活输入框；引用必须在创建线程使用。focus 的 Keep/Start/End/All 指定光标行为；select(anchor, caret) 使用 UTF-8 byte 偏移，必须均为完整 grapheme 边界，非法偏移返回 false。

autoFocus 只在首次布局请求一次，初始 disabled/不活跃分支不会在之后恢复时抢焦点。readOnly 仍允许焦点与选择。onFocus/onBlur 只通知实际焦点转换；可以在回调中请求另一引用焦点，转移在当前焦点事务结束后执行并检查身份/资格。同步卸载会解除绑定。

purpose 支持 Text、Name、Email、Username、Number；capitalization 支持 None、Sentences、Words、Letters，autocorrect 默认 true。这些是原生平台提示，不执行数值验证、大小写改写或 HTML 表单行为。Password 强制密码用途并关闭 autocorrect。reactive 提示变更在没有 IME preedit 时刷新会话，正在组合输入则延后至提交/取消；旧会话 stamp 不能继续提交。

系统输入端口是否提供对应键盘/纠正 UI 取决于实际平台；端口合同测试不能替代真实 OS 输入证据。

默认系统字体链保证 U+FFFD 替代字形；系统字体缺少该字形时追加显式配置的 bundled Latin fallback。未覆盖的 emoji/其他标量显示替代字形，编辑原文和统计保持不变。

## 变体与主题

三个单行组件的 `.variant(Prop<InputVariant>)` 支持 Outlined、Filled、Borderless、Underlined，默认 Outlined。三个尺寸保持等高；Underlined 只画底边，Borderless 在键盘焦点时显示 outline。Filled 使用独立背景与边框，半透明背景不会重复混色。disabled 与显式 Error/Warning 状态通过同一 Theme 推导。

InputTokenOverride 可覆盖 Filled 普通/hover 背景、Error/Warning 背景和文字色以及 focus_width。颜色更新只修改 retained material；变体更新保持 editor、选择、IME 会话、scene slot 与内容挂载身份。Compact 在两侧都有边框时合并接缝，无边框变体不产生虚假接缝。

Password 透传变体。Search 使用 Compact 连接输入框与 action，非 Outlined 变体使用 Text action；Filled action 使用普通/hover/pressed 背景。Underlined 的底边属于输入框，action 沿用 Text Button。Small Search 按 Input 的字号、行高、垂直 padding 与 border 设置共同最小高度，图标 action 的宽度至少等于高度。

## 统计与上限

`.showCount(Prop<bool>)` 控制 retained 计数标签；`.count(Prop<InputCountOptions>)` 设置软 max 和 Scalar/Grapheme 单位。默认 Scalar 与 maxLength 的原生 Unicode scalar 合同一致；例如 `e + combining acute` 是两个 scalar、一个 grapheme。countStrategy 可定义统计单位，countFormatter 接收 InputCountInfo 并返回 String。默认标签只显示数字及上限，Password 不显示原文。

```cpp
ryn::Input(ryn::InputProps{}.showCount()
               .count(ryn::InputCountOptions{20, ryn::InputCountUnit::Grapheme}));
```

软 max 只提示超限，不截断值；未设显式 status 时超限显示 Error，显式 Warning/Error 优先。统计显示上限优先使用 count.max，否则使用 maxLength。maxLength 始终是 scalar 硬限制，且不切开 grapheme。clear、自定义 suffix、counter 从左至右布局，隐藏 counter 不占尺寸；窄宽度下编辑区可以收缩到零。

exceedFormatter 接收用户候选和统计上限，只有超限时调用；其结果先按 editor 模式归一化换行（SingleLine 去除 CR/LF，MultiLine 将 CRLF/CR 转成 LF）、验证 UTF-8，再应用硬限制，然后作为一次历史事务发布。IME preedit 不统计或裁剪；提交、粘贴和删除使用同一入口。controlled authoritative 回写与 undo/redo 不经过该函数。formatter 可以重入或同步卸载；退休 owner 或被改写的事务不发布旧候选，异常保留原值/历史。自定义 formatter 不保证其返回值满足软上限，仍按返回值显示超限状态。

计数策略和标签只在 committed 值或配置变化时计算，空闲帧不重复调用。countFormatter 的异常保留上一个标签，统计与输入值继续更新；countStrategy 的显示异常保留上次统计，不在每帧重试。下次值/配置变化会重新计算。明确设置的自定义函数可读取原值，包括 Password，应自行决定显示内容。

## 清空与密码显隐

allowClear 控制清空动作是否存在/可见，clearDisabled 保留可见图标并禁用动作；clearIcon 接收 reactive IconSource，支持内置和 typed vector 图标。清空取消 preedit，发布一个空值编辑和 onChange，然后调用 onClear；回调拥有副本，onChange 中同步卸载仍可安全通知 onClear。空值、disabled、readOnly 不触发清空。

Password 的 visibilityToggle 与 toggleFocusable 均为 Prop<bool>，后者默认 true，使显隐按钮参与 Tab 顺序；鼠标操作保持输入框焦点/选择/IME。隐藏显隐按钮不占 suffix 空间。action 支持 Click（默认）/Hover，Hover 在每次 pointer enter 切换一次，离开只重新准备下一次进入；键盘 Enter/Space 仍可激活。iconRender 根据已接受的 visible 值返回 IconSource，受控 visibility 只发送候选，由外部回写决定是否显示。

Password 默认隐藏图标为 EyeInvisibleOutlined，显示图标为 EyeOutlined，已纠正旧实现的反向图标。Password(props, prefix, suffix) 使用同一 InputPrefix/InputSuffix typed slots；显隐图标放在自定义 suffix 前，统计仍放在最后。

## Search 操作

searchIcon 接收 reactive IconSource；未提供 SearchButtonContent 时 action 使用图标 slot，提供自定义内容时使用内容 slot。Search(props, button, prefix, suffix) 支持 typed 按钮及输入 affix。Enter/按钮通知 SearchSource::Input，并遵守 disabled/readOnly/loading/IME 规则；清空通知 SearchSource::Clear 和空候选，即使受控回写恢复了其他值，通知仍代表本次清空意图。loading 阻止提交，不阻止可编辑输入的清空通知。onClear 和 onSearch 均拥有回调副本。

鼠标点击 Search action 时，若所属输入框已聚焦则保留焦点、选择及 IME；其他情况下 action 正常获得鼠标焦点，键盘可始终按资格聚焦。

行为/样式基线核对 [6.6.5 Search](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/Search.tsx)、[Search style](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/style/search.ts) 和 [Password](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/Password.tsx)，052 的 source-contract.json 保存内容 hash。HTML/DOM 事件对象由原生 typed 回调取代。

## 收尾进度

053 的共享编辑基础增加固定 MultiLine 模式：初值、粘贴/提交、formatter 和 authoritative 值均将 CRLF/CR 规范为 LF；默认 SingleLine 合同不变。行光标映射按实际 TextMeasurement 生成，保留软折行的 upstream/downstream 位置，默认选择 downstream；二维命中、行边缘及保持期望 x 的行移动不在查询时分配。此基础仍是 LTR/CJK 合同。

`<ryn/text_area.hpp>` 提供 TextAreaProps/TextArea/TextAreaRef（共享 InputRef 合同），继承全部 Input 共用属性。`.rows(Prop<size_t>)` 默认 4；`.autoSize(TextAreaAutoSize{true, min_rows, max_rows})` 按实际硬/软换行决定高度，默认不启用，min_rows=1/max_rows 不限。`.wrap(Prop<bool>)` 默认 true，无 wrap 时只按 LF 分行。rows/min_rows 必须正数，max_rows 不得小于 min_rows；非法 reactive 配置拒绝并保留当前尺寸。`.resize(Prop<TextAreaResize>)` 支持 None/Vertical/Horizontal/Both，默认 Vertical；右下角 grip 使用 PointerRouter capture，autoSize 启用时不允许手工 resize，资格变化/卸载取消 capture。`.onResize(function<void(TextAreaSize)>)` 在实际边框逻辑尺寸首次确定或改变后通知，尺寸不包含下方计数；同步完成后使用回调副本及存活身份校验，可安全卸载自身。

TextArea 使用 Input Theme/四变体/三个尺寸，clear 停靠右上，计数右对齐放在输入边框下方。LayoutStyle 控制整个组件的外部尺寸（包含可见计数占用）；显式高度优先于 rows/autoSize，编辑区保留上下 padding 后占据剩余高度。尺寸/换行更新不会重建 editor、scene 或 ref。

Plain Enter 插入独立 LF 历史事务，primary Enter 调用 onSubmit，Tab 沿用正常焦点遍历。Home/End 使用当前视觉行，primary Home/End 使用整篇文档；上下及 Page 移动保留期望 x，Shift 保留选择 anchor，软折行边界左右键先切换视觉 affinity。点击/拖选按二维行映射；跨行 selection 与 preedit 下划线分别生成每行 coverage，selected glyph view 使用行 clip，仍共享一次 shaping。readOnly 可选择、复制和滚动，disabled 禁止编辑与拖动。

编辑/导航时露出 caret；用户 wheel 后保留所选滚动位置，直到下一次编辑/导航需要露出 caret。wrap=false 同时支持横向 wheel，边界未改变偏移时返回未消费，容器可继续滚动。滚动只修改 retained glyph geometry，不重复 shape/raster。IME 活跃时拥有导航/Enter/剪贴板快捷键，候选文本不进入 committed 计数；输入区域沿当前可见 caret 行同步。Gallery 追加八个 TextArea 稳定 ID 样本，包含四变体/count、autoSize、双向 resize/no-wrap、Dark Compact readOnly 与 disabled。[053 Windows 证据](../openspec/changes/053-20261003-add-native-text-area/evidence/windows/README.md)记录 Debug/Release 十轮 D3D12/DXIL 真实窗口、200 张 GPU 读回；Linux 独立待验收，OTP 与混合双向文字继续收尾。

052 已实现单行家族的变体、统计及操作配置，平台通用 Debug/Release 85/85 与 Windows 原生 Debug/Release 20/20、十轮 D3D12 窗口及 230 张 GPU 读回已通过；[Windows 证据](../openspec/changes/052-20261003-complete-native-input-features/evidence/windows/README.md)保存复核方式。Linux 保持独立待验收。Gallery 包含四变体、grapheme 超限、自定义裁剪、Email/ref、vector 清空、Hover Password、受控显隐以及 Dark Compact Filled/Small Underlined Search 的稳定 ID 样本。053 已接入 TextArea，OTP 和 RTL/混合文字视觉导航继续收尾，整个 Input 家族尚未标为完成。DOM/CSS/React 和 HTML 自动填充 API 不移植。
