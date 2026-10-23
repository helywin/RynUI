# Tag 与 Badge

`<ryn/tag.hpp>`、`<ryn/badge.hpp>`（也从 `<ryn/rynui.hpp>` 导出）。使用 C++20 typed Props/slots 与 `Prop<T>`；内容在首次挂载后保留。

## Tag

```cpp
auto visible = ryn::state(true);
auto checked = ryn::state(false);

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

`bordered` 默认为 true，关闭后仍保留原边框空间；对应锁定上游 outlined/filled 的边框区别。`closable` 默认为 false，`visible` 默认为 true。关闭按钮通过指针或 Enter/Space 调用 `onClose()`；它只发出请求，不改变 visible，调用者可拒绝或延迟写回。隐藏后不占正常布局、不接受输入，重新显示保留 slot 内容。CheckableTag 为完全受控 checked/onChange，默认 false；指针及 Enter/Space 产生下一值，调用者负责写回。`disabled` 同时阻止激活和焦点并采用禁用色；普通静态 Tag 不增加 Tab stop。

## Badge

```cpp
auto unread = ryn::state(105);

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

- count 为非负 `int`，可省略；`overflowCount` 默认 99，超过上限显示 `99+`。零默认隐藏，`showZero` 显示零。
- `dot` 隐藏数字；未设置 count 时直接显示点，显式零遵守 showZero。
- 有有效计数时优先展示计数/dot；计数省略或隐藏时使用 status 点。五种业务状态与 Tag 相同；None 表示无状态。
- `text` 为响应式 UTF-8 状态说明，显示在主体右边，与数字文本分离。隐藏的 indicator 同时隐藏说明。
- `size` 为 Default/Small。默认计数高度 20，小号 14，dot/status 为 6 logical px。
- 包裹 child 时，indicator 的中心锚定 child 右上角；`LogicalOffset{x,y}` 正值向右、向下。indicator 不扩大包装尺寸；若有 text，标签本身参与正常布局。独立使用时以 indicator/text 作为正常内容。
- Badge 自身没有 interaction 或 Tab stop，内部 Button/Input 保持原交互、主题语义和身份。indicator 服从祖先 clip。

无效 enum、负计数/上限和非有限 offset 在使用前抛出 `std::invalid_argument`，不发布部分属性。主题尺寸也在 immutable snapshot 发布前校验。

## Theme 与更新

`ThemeConfig::tag.tokens` 提供 default_background/default_color/default_border、checked_background/checked_color、close_color、padding_inline/icon_gap/close_size/border_width/border_radius/font_size/line_height。
`ThemeConfig::badge.tokens` 提供 background/count_color/text_color/outline_color、indicator_height/indicator_height_small/dot_size/status_size/padding_inline/font_size/font_weight。两者均支持继承、inherit=false、Default/Dark/Compact 及显式 component algorithm/seed。

颜色与 metrics/文字分组订阅；纯颜色、Tag 选中或 bordered 更新不测量内容。数字/说明/metrics 改变可重新布局，但不重跑 slots。LayoutStyle 仅用于外部布局。渲染复用 Text、Icon、retained surface 和 logical CPU scene v3，GPU ABI v2 不变。

## 首版范围与证据

源基线为项目锁定 Ant Design 6.6.5，不跟随当前网站。已检查本地 `components/tag/index.tsx`、`CheckableTag.tsx`、`style/index.ts`、`presetCmp.ts`、`statusCmp.ts` 与 `components/badge/Badge.tsx`、`style/index.ts`。style/index.ts SHA256 分别为 `9640325a03b0feac9571daefd4810d546287bbfb96150953a1feb21615dd06f3` 和 `d82221b8defc01a2c30ea6ca53dcf07fbe4943650ff90ae5a6417c4e10d35092`，与锁文件相核。

首版不提供 href、任意 ReactNode count、自定义 close slot、CheckableTagGroup、Badge.Ribbon、RTL 包装镜像、滚动数字、进退场和 processing 波纹动画。Processing 显示静态信息色；关闭采用显式受控请求。Gallery 与综合目录由父任务集成。

专用测试为 `rynui.tag_badge_public_api`、`rynui.tag_badge_theme`、`rynui.tag_badge_component`，使用 Windows MSVC headless preset 验证共同合同。真实 GPU 人工视觉、Linux/Wayland 和系统 IME 不属于本次证据。
