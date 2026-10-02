# Radio 与 RadioGroup

原生单选组件使用 typed Props 与保留标签。`checked`/`defaultChecked` 互斥；选中后不会取消，Space 只激活一次，Enter 不激活。`onChange(bool)` 只报告新的选中候选；`onClick(bool)` 也报告已选项点击。disabled 不发候选。

## 值与组合

`RadioValue = variant<String, double, bool>`，数值必须有限，类型互不混淆；`RadioSelection = optional<RadioValue>` 表示未选择。Group 最多 1024 项，值必须唯一。

```cpp
ryn::Signal<ryn::RadioSelection> selected{ryn::RadioSelection{false}};
ryn::Signal<std::vector<ryn::RadioOption>> options{std::vector<ryn::RadioOption>{
    {ryn::String{u8"a"}, ryn::String{u8"中文"}},
    {42.0, ryn::String{u8"数字"}},
    {false, ryn::String{u8"布尔值"}},
}};
ryn::RadioGroup(ryn::RadioGroupProps{}.options(options).selection(selected)
    .onValueChange([selected](const ryn::RadioValue& value) { selected.set(ryn::RadioSelection{value}); }));
```

已有 `.value(Prop<optional<String>>)` 和 `.onChange(const String&)` 保留；String 回调只报告 String，`onValueChange` 覆盖三类。两种受控入口和 defaultValue 互斥。受控候选等待回写；外部更新不发回调。

`options` 与 `RadioGroupContent` 互斥。手工成员通过最近祖先 Group 注册，允许被动 Flex/Space；嵌套 Group 独立。成员必须有 value，不能设置 checked/defaultChecked；Group disabled 与成员 disabled 取 OR。成员 onChange → Group 回调 → 成员 onClick 使用同一候选，回调内销毁安全。

```cpp
ryn::RadioGroup(ryn::RadioGroupProps{}.defaultValue(1.0), ryn::RadioGroupContent{[] {
    ryn::Radio(ryn::RadioProps{}.value(1.0), ryn::RadioLabel{[] { ryn::Text(u8"手工富标签"); }});
    ryn::Radio(ryn::RadioProps{}.value(2.0), ryn::RadioLabel{[] { ryn::Text(u8"第二项"); }});
}});
```

dynamic options 按类型值保留匹配项的组件、表面、标签与焦点；重排更新逻辑顺序。删除项取消 capture/焦点/资源，非受控选择清空且无回调。清空后可以恢复。非法更新在发布前拒绝，已挂载内容保持原状态。

## 焦点与方向

Group 具有一个 Tab 入口：当前聚焦可用项、可用选中项或第一个可用项。方向键循环选择并跳过 disabled；RTL 交换左右键逻辑，Space 选择当前项。受控模式方向键移动焦点并报告候选，选中状态仍等待回写。

`RadioRef` 是 owner-thread 的可复制引用，支持 bound/focus/blur。禁止绑定两个活成员，跨线程调用抛 logic_error；disabled/窗口失活时 focus 返回 false，销毁后引用解绑并可复用。autoFocus 与 ref 共用焦点管理。

direction 可在 Group 继承或成员覆盖；富标签只挂一次，标签内仅允许被动内容。视觉只能通过 Theme 控制，LayoutStyle 只控制外部布局。

## 按钮、主题与反馈

`RadioButton` 使用相同 Props/Label；Group 的 `optionType(RadioOptionType::Button)` 可将普通成员切换为按钮形态，保留原标签与组件身份。显式 RadioButton 始终保持按钮。Group 支持 `buttonStyle(Outline|Solid)`、`size(Small|Middle|Large)`、`block(bool)`、横纵和 RTL。直接相邻按钮共用一条边，仅组外侧角圆角；block 横向等宽、纵向填宽。被动包装内的成员保持包装布局。

ThemeConfig.radio 支持 tokens/seed/algorithm，继承方式与其他组件相同。上游公开 token 的原生字段包括 size/dot_size/dot_disabled、button_background/button_checked_background/button_color/button_padding_inline、button_checked_background_disabled/button_checked_color_disabled、button_solid_checked_color/background/hover/active_background、wrapper_margin_inline_end；dot/checked_background 适配内部 radioColor/radioBgColor。line_width、label_gap、字体/行高、按钮高度/圆角/Small 间距、focus 和 wave 为原生共同样式适配。上游不变：默认 16/6 圆点、40/32/24 按钮高度、15/7 间距；`seed.wireframe` 映射空心选中背景和 8 圆点。

颜色、metrics、effects 独立发布并进入 hash/JSON；Radio token 不影响 Switch，颜色变化不重排或重塑标签。尺寸/波纹参数必须有限且合法，圆点和线宽不得超过控件范围，wave opacity 必须在 [0,1]。

`wave(Prop<bool>)` 默认打开，采用有限动画；圆形反馈仅覆盖指示器，按钮反馈保留连接角，重启复用范围。结束、禁用、wave=false、motion=false、reduced motion、窗口失活与销毁均取消 deadline。先停止反馈并取消焦点/capture，再释放背景/表面，允许通知安全完成。

按钮背景与边界使用共同 logical rounded effects 的四个分区，保持 renderer packed GPU ABI。实现与平台证据以 [tasks](../openspec/changes/048-20261002-complete-native-radio-features/tasks.md) 为准。Web DOM/form/name/required/CSS/React 事件不移植。
