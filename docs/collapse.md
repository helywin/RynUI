# Collapse

原生保留式折叠面板，设计基线为锁定的 Ant Design **6.6.5**。通过 `<ryn/collapse.hpp>` 或 `<ryn/rynui.hpp>` 使用。

```cpp
ryn::Signal<ryn::CollapseKeys> keys{ryn::CollapseKeys{ryn::String{u8"settings"}}};
ryn::Collapse(
    ryn::CollapseProps{}
        .activeKeys(keys)
        .onChange([=](ryn::CollapseKeys next) { keys.set(std::move(next)); }),
    {ryn::CollapsePanel{
        u8"settings",
        ryn::CollapseTitle{[] { ryn::Text(u8"设置"); }},
        ryn::CollapseBody{[] {
            ryn::Input(ryn::InputProps{}.defaultValue(u8"保留的草稿"));
        }}}.extra(ryn::CollapseExtra{[] {
            ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"帮助"); });
        }})});
```

## 状态与 slots

- `CollapsePanel(key, CollapseTitle, CollapseBody)` 的 key 必须非空且唯一；`extra(CollapseExtra)` 可选，`disabled(Prop<bool>)` 禁止用户切换 header。
- `activeKeys(Prop<CollapseKeys>)` 是受控入口，`onChange(CollapseKeys)` 发送建议值；owner 拒绝更新时画面保持原状态。`defaultActiveKeys(CollapseKeys)` 只在挂载时读取，两者同时设置会抛出 `std::invalid_argument`。
- 未指定默认值时全部关闭。未知及重复 active key 被忽略；多开模式按输入顺序保留已知 key，`accordion(Prop<bool>)` 只显示第一个已知 key，再次激活可全部关闭。disabled 不禁止 owner 显式展开对应正文，也不禁用 extra 子控件。
- `size(Prop<ControlSize>)` 支持 Small/Middle/Large，`layout(LayoutStyle)` 只控制外部布局。其他视觉由 Theme 决定。
- 默认所有 title/body/extra 都在首次挂载时执行一次，包括未展开的正文；显式 lazy/destroy 策略见下。开合、普通 props 和 Theme 更新不会重挂载；隐藏正文不占布局空间，不进入命中和 Tab 顺序。
- 隐藏会退出正文焦点、输入 session、pointer capture 和子浮层。真实 Input editor 与草稿保持；重开不自动恢复原焦点或旧 popup。

## 交互

可用 header 是独立 Tab stop，支持 pointer、Enter 和 Space。Up/Down 循环移动可用 header 焦点，Home/End 到首尾，不改变展开状态。键盘焦点显示 Theme outline；箭头分别显示右向与下向。

extra 内 Button/Input 等原生交互保持独立，不触发 header toggle；无交互的普通标题/extra 区域属于 header 点击范围。Modal、Spin 与祖先 logical input gate 继续生效，组件不改写祖先 blocked 状态。

## Theme

`ThemeConfig::collapse` 提供 typed tokens、组件 seed 与显式 `algorithm`，默认继承父 Theme；支持 Default/Dark/Compact 和 `inherit=false`。颜色、metrics、typography、effects 通过独立订阅失效；纯颜色不重新测量。

颜色字段包括 `header_background`、`body_background`、`header_text`、`body_text`、`disabled_text`、`border`、`focus_outline`。尺寸字段包括三种尺寸的 `header_block`/`header_start`/`header_end`/`body_padding`（后缀 `_small`、`_large`）、`arrow_size`、`header_gap`、`border_width`、`border_radius`。字体字段为 `font_family`、`font_weight`、`font_size`、`line_height`、`header_font_size_large`、`header_line_height_large`；焦点字段为 `focus_width`、`focus_offset`。长度 override 使用 `dp(...)`。

锁定默认 header 垂直/左/右 padding 分别为 Small `8/8/12`、Middle `12/16/16`、Large `16/16/24`；正文 padding `12/16/24`，普通字号/行高 `14/22`，Large header `16/24`。边框宽度计入根尺寸。

## 动态面板、策略与 motion

`Collapse(props, Prop<vector<CollapsePanel>>)` 按非空唯一 String key 保留 title/body/extra。重排移动同一实例；删除清理整个 panel。普通 Prop/Theme 更新不执行内容工厂。同 key 的新工厂默认保留旧实例，只有 `titleRevision`、`bodyRevision`、`extraRevision` 变化才局部替换相应 slot；工厂异常退役该 Collapse，其他组件保持有效。

旧 vector 调用和新动态调用默认预挂载正文，保留旧版未展开 Input 草稿语义；上游默认 lazy 对应显式 `lazy()`。`forceRender()` 保持正文预挂载并优先于 `destroyOnHidden()`。销毁策略可按 panel 覆盖；关闭立即清理焦点、preedit、capture 和子 popup，正文仍按当前高度裁剪绘制，有限过渡结束后才销毁。反向从当前进度继续；hidden、window inactive、reduced motion 与析构停止动画，idle 不调度帧。

`bordered(false)`、`ghost()`、三种 size、`rtl()`、`visible()`、`collapsible(Header/Icon/Disabled)`、`showArrow`、`expandIconPlacement(Start/End)` 均为 typed 原生入口。Disabled 禁止用户切换，应用 activeKeys 仍为权威，extra 内控件独立。`expandIcon` 工厂收到 key 与 reactive `Binding<bool> active`，挂载一次，自行声明原生图标；默认图标使用真实旋转过渡。`animated(false)` 可关闭过渡。

浏览器 DOM/CSS/React aliases 不移植；String key 对应原生身份，不混用浏览器数字 key 的隐式字符串化。后台 GPU 验收与物理 IME、Windows、AT 分开记录，见 change 107 的精简 evidence。

## Gallery

Token Gallery 的 LiveSamples 末尾提供 `gallery.collapse.retained` 设置面板：基本设置保留真实 Input 草稿和受控 Select 浮层，高级设置保留独立草稿并默认禁用。外部按钮切换 accordion、启用高级面板、收起全部或展示正文选项；header 操作回写受控展开值，独立帮助 Button 更新反馈且不切换面板。关闭后正文退出输入和子浮层，重新展开继续编辑原草稿。

074 的 `token_gallery_frame --panels-only` 源码场景覆盖 1200×900 整页滚动后的操作、未聚焦浮层通过外部按钮键盘关闭、重复开合、主题/宽度变化与保留身份；源码就绪不代表运行通过，实际构建、inventory 和测试结果由 074 验收记录确认。原生 GPU、Linux 和人工 IME/视觉仍独立验收。

```cpp
ryn::Signal<bool> reverse{false};
ryn::Collapse(ryn::CollapseProps{}.lazy().ghost(), ryn::bind([reverse] {
    std::vector<ryn::CollapsePanel> panels{
        {u8"draft", ryn::CollapseTitle{[] { ryn::Text(u8"草稿"); }},
         ryn::CollapseBody{[] { ryn::Input(ryn::InputProps{}); }}}};
    if (reverse.get()) { panels.push_back({u8"more", ryn::CollapseTitle{[] {}}, ryn::CollapseBody{[] {}}}); }
    return panels;
}));
```

107 新增动态面板页内样例：重排实际 Input 草稿、切换 ghost，并展示显式 lazy 与按面板 destroyOnHidden；合成示例数据在页面标明。
