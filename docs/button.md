# Button

Button 使用 retained content、统一 input/focus 路由和 Theme；更新选择器不会重跑内容 slot。旧 `Button(props, content)` 以及 Default/Primary/Danger/Text 类型保留行为。

## 颜色与变体

```cpp
ryn::Signal<ryn::ButtonColor> color{ryn::ButtonColor::Blue};
ryn::Button(ryn::ButtonProps{}
                .color(color)
                .variant(ryn::ButtonVariant::Dashed)
                .ghost(true)
                .onClick([] { /* native action */ }),
            [] { ryn::Text(u8"Action"); });
```

`ButtonVariant` 支持 Outlined、Dashed、Solid、Filled、Text、Link。`ButtonColor` 支持 Default、Primary、Danger，以及 Blue、Purple、Cyan、Green、Magenta、Pink、Red、Orange、Yellow、Volcano、Geekblue、Lime、Gold；Pink 与 Magenta 共用默认色板。公开选择器均接受 reactive `Prop<T>`。

`type` 提供默认颜色/变体：Default→Outlined、Primary/Danger→Solid、Text→Text、Dashed→Dashed、Link→Link。显式 `color` 优先于 `danger` 和 `type`；显式 `variant` 优先于 `type`，两项可以独立设置。`ghost` 将 Solid 转为 Outlined，让有背景的变体使用透明背景；Text/Link 保留原有语义。禁用状态优先于 hover、pressed 和 loading；loading 抑制激活并保留原有进度指示。

## Theme 与绘制

颜色来自 `ButtonThemeToken::variants` 的缓存色板，主题和组件 seed 负责主色/危险色派生；组件内不保存预设 RGB。`ThemeConfig::button.tokens.colors[static_cast<std::size_t>(ButtonColor::Blue)]` 可覆盖 base/hover/active/light/light_hover/light_active/solid_text/shadow。新选择器继承旧主色/危险色 token override，父 Theme 的预设色 override 也会保留。

`border_width`、`dash_length`、`dash_gap` 控制真实虚线的几何。虚线使用共同 RoundedEffect outline 和互不重叠的 clip，ghost 间隙保持透明；不会用背景覆盖实线，也不增加 GPU ABI 字段。单个按钮最多生成 4096 个虚线片段，超出时明确抛出 `std::length_error`。颜色更新只更新 Material；虚线长度、间距或边框宽度更新请求 Geometry。

## 实施与验收

044 的颜色/变体阶段以 Windows MSVC `windows-msvc-headless` Debug/Release 验证 16×6 组合、状态优先级、透明间隙、Theme identity/继承、普通颜色更新不测量/不重排文本，以及销毁回调和资源清理。图标/形状/focus ref/loading delay/wave 在该 change 的后续阶段实施；真实 Windows GPU 和 Linux native 验收分别记录，headless 结果不代替原生平台证据。

公开 API 不移植 HTML/DOM/CSS 属性。完整计划和阶段证据见 [044](../openspec/changes/044-20261002-complete-native-button-variants)。
