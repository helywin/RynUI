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

044 以 Windows MSVC `windows-msvc-headless` Debug/Release 验证颜色/状态矩阵、透明间隙、Theme identity/继承、普通颜色更新不测量/不重排文本，以及 retained slots、布局、focus ref、deadline、wave 和资源清理。真实 Windows GPU 和 Linux native 验收分别记录，headless 结果不代替原生平台证据。

## 图标、形状与加载

```cpp
ryn::ButtonRef action;
ryn::Signal<bool> busy{false};
ryn::Button(ryn::ButtonProps{}
                .ref(action)
                .autoFocus(true)
                .loading(busy)
                .loadingDelay(ryn::Duration::milliseconds(150))
                .iconPlacement(ryn::ButtonIconPlacement::End)
                .shape(ryn::ButtonShape::Round)
                .block(true),
            ryn::ButtonContent{[] { ryn::Text(u8"Submit"); }},
            ryn::ButtonIcon{[] { ryn::Text(u8"+"); }},
            ryn::ButtonLoadingIcon{[] { ryn::Text(u8"…"); }});

// Icon-only 入口明确声明 slot，旧 Button(props, lambda) 调用保持无歧义。
ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Circle),
            ryn::ButtonSlots{.icon = ryn::ButtonIcon{[] { ryn::Text(u8"+"); }}});
```

也可在 slot 内声明 `Icon` 或 reactive Text。常规/加载图标和内容各挂载一次；loading 替换可见图标，不累加两个图标或多余 gap。未配置自定义加载 slot 时显示内置 spinner，位置遵守 Start/End。`ButtonSlots` 可单独声明 content/icon/loading；无任何 slot 或空 callable 在获取资源前拒绝。

shape 支持 Default/Circle/Round/Square，三档 `ControlSize`；icon-only 默认宽度至少 control height，Circle 使用半高圆角，Round 使用半高圆角，Square 的圆角为零。`block` 填满有限父约束；无限约束使用自然宽度，显式 `LayoutStyle.width` 优先。

`ButtonRef` 的 `focus()` / `blur()` / `bound()` 只允许 owner thread 使用。disabled 或失活窗口不能通过 ref 获取焦点；loading 保留焦点。销毁后方法返回 false，同一个 ref 可以重新绑定新一代组件，重复绑定会失败。`autoFocus` 只在首次挂载请求键盘焦点，不随属性更新再次执行。

`loadingDelay` 接受非负 `Duration`；等待期间仍可操作，到期才进入 loading。恢复 false、重配 delay、销毁会取消旧截止时间；非零重配从当前 host 单调时间重新计时，零延迟立即生效。实现使用窗口 deadline/tick，无阻塞等待或平台私有定时器。

## 点击反馈

有边框变体默认启用有限 wave，`.wave(false)` 可关闭。每次成功 activation 在共同 RoundedEffect 中扩散并淡出，重复点击重启一条 scalar 通道；不会累积多个波纹，不改变 measure、命中范围或焦点。默认中性色使用主题 hover 主色，彩色按钮使用当前主题颜色。`wave_spread` / `wave_width` / `wave_opacity` 控制几何与透明度；持续时间和 easing 采用 Theme 的 slow / ease_out motion token。

disabled、loading、Text/Link、motion=false/reduced、窗口失活和销毁会取消 wave。opacity/width 为零也停止动画。结束或取消时移除波纹 effect 与未来 wave deadline，保留组件拥有的 range 供下一次点击复用；组件销毁时释放该 range。activation 回调可安全销毁本组件或父组件。

公开 API 不移植 HTML/DOM/CSS 属性。完整计划和阶段证据见 [044](../openspec/changes/044-20261002-complete-native-button-variants)。
