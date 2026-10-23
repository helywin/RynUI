# Card

059 正在实现原生 Card。当前阶段先提供 typed API 和完整 Card Token；容器挂载、Gallery 和原生平台验收按 tasks 分阶段记录，不能把公开声明视为可运行组件。

```cpp
ryn::Card(ryn::CardProps{}.title(u8"账户").hoverable()
    .layout(ryn::LayoutStyle{}.width(ryn::dp(320))),
    ryn::CardContent{[] { ryn::Text(u8"账户信息"); }},
    ryn::CardSlots{
        .extra = ryn::CardExtra{[] {
            ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"编辑"); });
        }},
        .actions = ryn::CardActions{[] { ryn::Text(u8"底部操作区域"); }}
    });
```

CardSize 为 Default/Small，CardVariant 为 Outlined/Borderless；title、size、variant、hoverable、loading 支持 `Prop<T>`。标题可用 typed CardTitle slot 替代文本；另有 CardExtra、CardCover、CardActions，正文为 CardContent。slot 的类型不可混用；title slot 存在时优先于 title 属性。

视觉覆盖使用 `ThemeConfig::card.tokens`，包括背景/标题/extra/边框/分隔线/占位颜色、两尺寸 header/body padding 与高度、字体/字重、圆角和普通/hover shadow；不放入 LayoutStyle。默认配方来自锁定 Ant Design 6.6.5，small body/header padding 固定12；主题支持继承/reset、独立 component seed/algorithm 与四组精确失效域。

首版目标是内容容器，不包含 Grid、Meta、内置 Tabs、异步图片加载、骨架动画或 Web/DOM 样式接口。cover 可放应用自己的原生内容。loading 后续将保留已挂载正文的编辑状态并切换静态占位，恢复不自动夺焦点。
