# Card

059 提供原生 Card 容器，复用窗口服务、保留子树与共同 logical scene。平台通用、Gallery 和原生平台验收按 tasks 分阶段记录，未通过的平台项保持待验收。

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

header、cover、body、actions 按顺序自然布局，header 只有标题或 extra 时出现；extra 先测量自身宽度，title 使用剩余空间。普通标题单行省略并保留完整源文本，自定义 CardTitle slot 自行控制排版。正文使用对应尺寸的 padding，actions 默认将内容居中；包含多个子控件时，可在 CardContent/CardActions 中使用 Flex/Space 排列。

loading 保留已挂载正文的编辑状态并切换静态三行占位，隐藏正文焦点、输入会话、命中与嵌套弹层；header/cover/actions 继续显示。恢复后保留原 Input/Select 等子组件 identity 和数据，不自动夺焦点。hoverable 在指针进入卡片背景或任意子控件时显示悬停阴影，不拦截内部 Button/Link；Card 本身没有 Tab 入口。普通标题、主题、尺寸和状态更新不会重新执行 slots。

背景、分隔线、占位和边框复用共同 rounded effects，保留逻辑平移、透明度和祖先矩形裁剪，不增加 renderer ABI 或私有上传。销毁会清理子组件、interaction、订阅与效果资源；静态占位不持续请求动画帧。

首版不包含 Grid、Meta、内置 Tabs、异步图片加载、骨架动画或 Web/DOM 样式接口。cover 可放应用自己的原生内容；Card 未提供对任意后代内容的圆角遮罩。

Gallery 已提供7个真实 Card 和 loading 切换操作；平台通用完整 headless、Gallery Recording 与 Windows MSVC Debug/Release 的20次 D3D12/DXIL主题/缩放窗口验收已通过。见 [Windows证据](../openspec/changes/059-20261023-add-native-card/evidence/windows.md)。自动输入不替代人工系统IME，Linux原生验收仍待实际机器完成。
