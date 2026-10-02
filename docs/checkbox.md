# Checkbox 原生 API

Checkbox 支持受控 `checked(Prop<bool>)` 或非受控 `defaultChecked(bool)`，两者不可同时提供。`indeterminate` 独立于 checked；disabled 抑制 pointer 与 Space 激活。CheckboxLabel 是只挂载一次的 typed slot，支持 Text、Icon 和被动布局；交互子控件会拒绝并回滚挂载。

```cpp
ryn::Signal<ryn::CheckboxValues> selected{ryn::CheckboxValues{ryn::String{u8"desktop"}}};
ryn::CheckboxGroup(ryn::CheckboxGroupProps{}
    .options(std::vector<ryn::CheckboxOption>{
        {ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
        {1.0, ryn::String{u8"数字"}},
        {true, ryn::String{u8"布尔"}, true}})
    .value(selected)
    .onChange([&](const ryn::CheckboxValues& value) { selected.set(value); }));
```

CheckboxValue 是 String、有限 double、bool 的 variant，不同类型的值互不相等。Group 的 value/defaultValue 互斥；重复选中值、重复 option value、非有限数字及超过 1024 个 options 明确拒绝。受控 Group 只向 onChange 提供候选值，展示继续服从 value；外部更新不触发回调。候选值按已注册选项顺序排列并过滤不存在的值；非受控删除选项时静默清理选中值。

`options(Prop<std::vector<CheckboxOption>>)` 按 value 保留现有组件、文字与交互 identity。标签和 disabled 可独立更新；重排同时改变布局与 Tab 顺序；删除清理焦点、鼠标捕获、文字和 scene 资源。空 options 可恢复。无效输入不会替换已经发布的选项。

也可使用 CheckboxGroupContent 手动组合，不能与 options 同时提供。组内 Checkbox 必须指定 value，不能再提供 checked/defaultChecked；`skipGroup(true)` 保持独立。被动布局不阻断最近祖先 Group 的注册，嵌套 Group 独立管理自己的成员。group disabled 与成员自身 disabled 按逻辑或合并。Group 支持 Horizontal/Vertical orientation，自身不参与 Tab 和激活。

有效激活执行成员 onChange，再执行 Group onChange；回调候选提前复制，成员回调销毁整个 Group 后仍可安全完成 Group 回调。DOM、表单 name/required、HTML 属性和 React 专用接口不移植。实施与平台证据见 `047-20261002-complete-native-checkbox-group`。
