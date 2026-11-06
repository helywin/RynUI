# Tag 与 Badge

`<ryn/tag.hpp>`、`<ryn/badge.hpp>`（也从 `<ryn/rynui.hpp>` 导出）。使用 C++20 typed Props/slots 与 `Prop<T>`；内容在首次挂载后保留。

## Tag

```cpp
ryn::Signal<bool> visible{true};
ryn::Signal<bool> checked{false};

ryn::Tag(
    ryn::TagProps{}
        .color(ryn::TagColor{ryn::Status::Success})
        .closable()
        .visible(visible)
        .onClose([=] { visible.set(false); }),
    ryn::TagContent{[] { ryn::Text(u8"Connected / 已连接"); }},
    ryn::TagSlots{.icon = ryn::TagIcon{[] {
        ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
    }}});

ryn::CheckableTag(
    ryn::CheckableTagProps{}
        .checked(checked)
        .onChange([=](bool next) { checked.set(next); }),
    ryn::TagContent{[] { ryn::Text(u8"Online devices / 在线设备"); }});
```

`color` 接受 `Prop<TagColor>`；`TagColor` 是 `PresetColor | Status | Color`。
预设包含 Default、Magenta、Red、Volcano、Orange、Gold、Lime、Green、Cyan、Blue、Geekblue、Purple、Pink。
Status 包含 None、Default、Success、Processing、Error、Warning；None/Default 使用中性样式。自定义 Color 作为填充色并使用白色文本。

`bordered` 默认为 true，关闭后仍保留原边框空间；对应锁定上游 outlined/filled 的边框区别。`closable` 默认为 false，`visible` 默认为 true。关闭按钮通过指针或 Enter/Space 调用 `onClose()`。显式设置 visible 时保持旧的受控请求语义，拒绝写回不改变已接受状态；未设置 visible 时默认关闭并隐藏。新增 `onClose(TagCloseEvent&)` 可调用 `preventDefault()` 取消。隐藏后不占正常布局、不接受输入，重新显示保留 slot 内容。CheckableTag 的 checked/onChange 保持完全受控，默认 false；显式 `defaultChecked` 是原生扩展的默认值模式，指针及 Enter/Space 产生下一值，调用者负责写回。`disabled` 同时阻止激活和焦点并采用禁用色；普通静态 Tag 不增加 Tab stop。

## Badge

```cpp
ryn::Signal<int> unread{105};

ryn::Badge(
    ryn::BadgeProps{}.count(unread).overflowCount(99).offset(ryn::LogicalOffset{2, 0}),
    ryn::BadgeContent{[] {
        ryn::Button(ryn::ButtonProps{}.onClick([] { /* open inbox */ }),
                    [] { ryn::Text(u8"Inbox / 收件箱"); });
    }});

ryn::Badge(ryn::BadgeProps{}.status(ryn::Status::Processing).text(u8"Syncing / 同步中"));
ryn::Badge(ryn::BadgeProps{}.count(0).showZero());
ryn::Badge(ryn::BadgeProps{}.dot());
```

- 旧 count(int) 为非负整数，可省略；新增 String 与 typed countValue 见下方 change107 范围。`overflowCount` 默认 99，超过上限显示 `99+`。零默认隐藏，`showZero` 显示零。
- `dot` 隐藏数字；未设置 count 时直接显示点，显式零遵守 showZero。
- 有有效计数时优先展示计数/dot；计数省略或隐藏时使用 status 点。五种业务状态与 Tag 相同；None 表示无状态。
- `text` 为响应式 UTF-8 状态说明，显示在主体右边，与数字文本分离。隐藏的 indicator 同时隐藏说明。
- `size` 为 Default/Small。默认计数高度 20，小号 14，dot/status 为 6 logical px。
- 包裹 child 时，indicator 的中心锚定 child 右上角；`LogicalOffset{x,y}` 正值向右、向下。indicator 不扩大包装尺寸；若有 text，标签本身参与正常布局。独立使用时以 indicator/text 作为正常内容。
- Badge 自身没有 interaction 或 Tab stop，内部 Button/Input 保持原交互、主题语义和身份。indicator 服从祖先 clip。

无效 enum、旧 int 负计数/负上限和非有限 offset 在使用前抛出 `std::invalid_argument`，不发布部分属性。主题尺寸也在 immutable snapshot 发布前校验。

## Theme 与更新

`ThemeConfig::tag.tokens` 提供 default_background/default_color/default_border、checked_background/checked_color、close_color、padding_inline/icon_gap/close_size/border_width/border_radius/font_size/line_height。
`ThemeConfig::badge.tokens` 提供 background/count_color/text_color/outline_color、indicator_height/indicator_height_small/dot_size/status_size/padding_inline/font_size/font_weight。两者均支持继承、inherit=false、Default/Dark/Compact 及显式 component algorithm/seed。

颜色与 metrics/文字分组订阅；纯颜色、Tag 选中或 bordered 更新不测量内容。数字/说明/metrics 改变可重新布局，但不重跑 slots。LayoutStyle 仅用于外部布局。渲染复用 Text、Icon、retained surface 和 logical CPU scene v3，GPU ABI v2 不变。

## change107 的原生补齐

固定上游为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，核对 Avatar/Badge/Tag 的 API、类型、styles、demos 与 tests。Badge 上游公开组合仅有 Ribbon，不存在 BadgeGroup，因此不虚构这个 API。`BadgeRibbon` 是公开 Ribbon 的原生命名；`CheckableTagGroup` 对应真实上游 CheckableTagGroup；`TagGroup` 是上游 draggable demo 的原生 retained 组合，使用独立拖动手柄保护富编辑器的鼠标选择。

Badge 保留非负 int count 的旧验证，增加 String count 和 `countValue(Prop<BadgeCountValue>)`，后者支持空值、int、有限 double、String 和共享 BadgeRenderer。double 支持小数/负数；字符串是任意 UTF-8 内容。数字超过 overflowCount 显示上限加号，零遵循 showZero，富计数使用真实 retained slot。`contentRender` 替换局部内容工厂，计数更新不重建子 Button/Input。rtl 镜像锚点与水平 offset；visible 隐藏分支。

数字变化通过裁剪容器中的前后两行数字进行有方向的有限滚动，中间帧有实际位置变化；新目标/反向请求重新开始当前请求的有限过渡。Processing 仅在有可见几何、活动窗口且 Theme/motion preference 允许时产生波纹和下一帧期限；隐藏、失活、reduced motion、离页均停止。没有假进度百分比。Ribbon 支持 text/rich text、保留子内容、Start/End、RTL、动态工厂和 Theme 视觉。

Tag 支持 Outlined/Filled/Solid、原有 bordered/color/disabled/icon、typed close slot 与 RTL。enter/exit 使用有限裁剪包络，Theme 的 fast 时长和 reduced motion 控制过渡；closing 立即屏蔽焦点、捕获、IME 和 owned popup，再完成 paint。默认保留关闭的内容；`destroyOnClose()` 在动画完成或直接收敛时销毁精确 owner。回调可取消、重入和自销毁；异常会退役 owner，不继续访问失效状态。

`TagGroupProps::items` 接受响应键控 TagGroupItem；相同键的数据更新/排序保留富 Input 草稿、选择和 refs，只有删除键或该键工厂身份改变才替换其内容。整批先校验 key/color；用户工厂失败会准确退役组。`draggable` 使用 5 logical px 阈值、PointerRouter capture、取消和可见裁剪几何，独立手柄支持左右键重排，RTL 反向解析逻辑前后。设置 `onReorder` 时只发送候选 key 顺序，必须由应用更新 items 接受；拒绝不改顺序。未设置回调时组内部接受排序。disabled、移除活动项、窗口失活、输入边界改变和离页清理捕获。

CheckableTagGroup 使用同一 retained 引擎，`value/defaultValue`、`multiple`、`onChange` 表达受控/默认选择；单选最多一个值，空集表示未选。所有组内部控件和 Tag 私有内容使用 FormPrivateEditorScope；显式嵌套 FormItem 仍可正常绑定，外层后续公开控件保留其自动绑定与 Enter 权威。

新增 Badge 和 Tag 独立 Gallery 样例展示实际回调、滚动计数/Processing/Ribbon/任意文本、取消关闭、受控选择、接受/拒绝重排与真实草稿。旧样例与 458 个冻结身份不变。实际平台/preset/命令和通过数量分别记录在 change107 `evidence/badge-linux.md`、`evidence/tag-linux.md`。HEADLESS/Recording 不表示 Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器通过。DOM/CSS/React/portal、href/target 和 Web ref 不移植。
