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

048 后续阶段还将补按钮形态、独立 Theme 和 wave；当前文档的实现与平台证据以 [tasks](../openspec/changes/048-20261002-complete-native-radio-features/tasks.md) 为准。Web DOM/form/name/required/CSS/React 事件不移植。
