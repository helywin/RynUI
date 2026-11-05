# Segmented

`<ryn/segmented.hpp>` 提供少量保留式动态选项的原生单选控件。视觉参考固定为 Ant Design 6.6.5，不引入 Web 依赖。

```cpp
ryn::Signal<ryn::String> view{ryn::String{u8"daily"}};
ryn::Segmented(
    ryn::SegmentedProps{}
        .value(view)
        .block()
        .onChange([=](ryn::String key) { view.set(std::move(key)); }),
    {{u8"daily", u8"日"}, {u8"weekly", u8"周"}, {u8"monthly", u8"月"}});
```

旧 String 调用沿用 Tabs 的 key 约定；typed 数值入口见下文。key 必须非空且唯一，动态 `Prop<vector<SegmentedOption>>` 按稳定 key 增删和重排。`value(Prop<String>)` 是受控模式，应用不回写时选择不变；`defaultValue(String)` 只用于非受控初值，两者冲突会在挂载前抛出 `std::invalid_argument`。没有初值时选择首个未禁用选项，空列表或全禁用列表无选择。未知显式 key 保留但没有选中项，显式选中的禁用项仍显示选中；禁用更新不会另选一项或触发回调。`onChange(String)` 仅报告用户请求的不同可用 key，回调允许销毁控件或祖先。

`SegmentedOption::label(Prop<String>)` 与 `disabled(Prop<bool>)` 原地更新。`content(SegmentedLabel)` 提供展示型富标签并替换默认文本；`icon(SegmentedIcon)` 提供前置展示型图标。slot 挂载一次，可在内部读取自己的 Signal，继承控件的语义文字颜色与字体。标签/图标 slot 支持独立 Button/Input 与嵌套 FormItem，子控件的输入不触发父选项选择。使用 rich content 时，外层 label 属性不替换 rich content。

```cpp
ryn::Segmented(
    ryn::SegmentedProps{}.defaultValue(u8"list"),
    {ryn::SegmentedOption{u8"list", u8"列表"}
         .icon(ryn::SegmentedIcon{[] {
             ryn::Icon(ryn::IconProps{}.name(ryn::IconName::SearchOutlined));
         }}),
     {u8"detail", u8"详情"}});
```

`block(Prop<bool>)` 水平填满有限父宽并等分实际 item 宽度；自然模式按内容分配宽度。`vertical(Prop<bool>)` 纵向排列，每项使用共同宽度，block 时填满父宽。小、中、大尺寸使用 `ControlSize::Small/Middle/Large`，默认 Middle；标准单行高度为 24/32/40 logical px。受约束的长标签按可用宽度换行，可增加高度。宽高为零时 item 保持在零分配范围内。`LayoutStyle` 只管理外部布局。

每组只有一个逻辑 Tab 入口：优先当前可用焦点项，否则选中可用项，再否则首个可用项。水平 Left/Right、纵向 Up/Down 循环跳过禁用项并请求选择；Home/End 定位首尾可用项，Enter/Space 请求当前项，重复激活键不重复通知。受控拒绝不阻止焦点移动。pointer 释放到项外取消选择，禁用、窗口失活、Modal/Spin 阻挡和销毁会取消 capture/焦点资格。Modal/Spin 不覆盖用户 disabled 值。

`ThemeConfig::segmented` 提供独立颜色、metrics 与 effects tokens；支持父主题继承、`inherit=false` 和组件 algorithm/seed。selected 阴影继承 `box_shadow_tertiary`，focus-visible 使用共同 hollow ring。纯颜色更新只改变 Material，不重跑 slot、测量或重建 scene。所有视觉使用共同 logical scene 与 retained effects。

`SegmentedValue = variant<String, double>` 提供严格区分 String/number 的值。旧 String value/onChange 保持源码兼容；数值选项通过 `SegmentedOption(key, label).value(1.0)` 表达稳定身份和值，typed authority 使用 `value(Prop<SegmentedValue>)`，事件为 `onValueChange`。数值必须有限，key 和 value 都不可重复。移除非受控选中项回退首个可用选项；受控的未知/禁用值保持权威。

`rtl` 实际镜像水平排列与键盘方向，`vertical` 控制纵向，`shape(Default/Round)`、`visible` 和每项 `visible` 支持响应更新。`contentRevision` / `iconRevision` 显式局部替换对应工厂，其他 slot、Input draft、ref 和 item 身份保持。非法集合在修改前拒绝；工厂异常退役该控件并清理新旧资源。

选中 pill 在实际测量的矩形之间执行有限过渡，中途反向从当前矩形继续。resize、隐藏、窗口失活、reduced motion 停止；selected shadow 与 focus outline 继续来自 Theme，纯颜色不重挂 slots。Web name/DOM/CSS/React 接口与 OS accessibility bridge 不在原生 UI 合同内；平台证据见 change 107。

## Gallery

LiveSamples 末尾的 `gallery.segmented.controlled` 展示带图标的横向等宽选择，与小尺寸纵向 `gallery.segmented.vertical` 共享受控 day/week/month 值和实际反馈。外部按钮原地更新日视图标签、切换周视图禁用；大尺寸 `gallery.segmented.rich` 展示保留图标/文字组合及独立非受控选择。

074 的 `token_gallery_frame --panels-only` 源码场景覆盖真实指针、横纵键盘跳过响应式禁用项、共享回写、实际等宽与可见标签、主题/宽度变化和保留 item/文字身份。实际运行结果、inventory 和 Recording 的验证边界由 074 验收记录确认；不据此声明原生 GPU、Linux 或人工视觉通过。

```cpp
ryn::Signal<ryn::SegmentedValue> selected{ryn::SegmentedValue{1.0}};
ryn::Segmented(ryn::SegmentedProps{}.value(selected).rtl().shape(ryn::SegmentedShape::Round)
    .onValueChange([selected](ryn::SegmentedValue next) { selected.set(std::move(next)); }),
    {ryn::SegmentedOption{u8"one", u8"一"}.value(1.0), ryn::SegmentedOption{u8"two", u8"二"}.value(2.0)});
```

107 新增 typed 数值、Round 与 RTL 动态样例，按钮重排稳定 key 并切换方向；实际回调结果与有限 pill 过渡可见，数据标明为合成示例。
