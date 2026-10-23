# Segmented

`<ryn/segmented.hpp>` 提供少量固定选项的原生单选控件。视觉参考固定为 Ant Design 6.6.5，不引入 Web 依赖。

```cpp
auto view = ryn::state(ryn::String{u8"daily"});
ryn::Segmented(
    ryn::SegmentedProps{}
        .value(view)
        .block()
        .onChange([=](ryn::String key) { view.set(std::move(key)); }),
    {{u8"daily", u8"日"}, {u8"weekly", u8"周"}, {u8"monthly", u8"月"}});
```

值沿用 Tabs 的 `String` key 约定。key 必须非空且唯一，选项列表在挂载后固定。`value(Prop<String>)` 是受控模式，应用不回写时选择不变；`defaultValue(String)` 只用于非受控初值，两者冲突会在挂载前抛出 `std::invalid_argument`。没有初值时选择首个未禁用选项，空列表或全禁用列表无选择。未知显式 key 保留但没有选中项，显式选中的禁用项仍显示选中；禁用更新不会另选一项或触发回调。`onChange(String)` 仅报告用户请求的不同可用 key，回调允许销毁控件或祖先。

`SegmentedOption::label(Prop<String>)` 与 `disabled(Prop<bool>)` 原地更新。`content(SegmentedLabel)` 提供展示型富标签并替换默认文本；`icon(SegmentedIcon)` 提供前置展示型图标。slot 挂载一次，可在内部读取自己的 Signal，继承控件的语义文字颜色与字体。标签/图标 slot 应为被动展示内容，不嵌套 Button/Input 等独立交互控件。使用 rich content 时，外层 label 属性不替换 rich content。

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

首版没有滑块/pill 动画、动态增删/重排选项、RTL 容器排列、平台 accessibility adapter 或完整 Web Props/Token aliases。这里的键盘可操作性不代表已接入 OS screen reader。平台通用证据在 change 071；未声明 native GPU、Linux、系统字体或人工 IME 验收。
