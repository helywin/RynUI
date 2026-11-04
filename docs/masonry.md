# Masonry 原生瀑布流

公开入口 `<ryn/masonry.hpp>` 提供 `Masonry(MasonryProps<Data>)`、`MasonryItem<Data>`、`MasonryItemView<Data>`。items 是 reactive `Prop<std::vector<MasonryItem<Data>>>`；每项的 key 为非空 String 或有符号整数，字符串 `"1"` 和整数 1 是不同键。重复/空键在修改挂载内容前拒绝。

```cpp
struct CardData { ryn::String title; float height; };
ryn::Signal<std::vector<ryn::MasonryItem<CardData>>> cards{
    std::vector{ryn::MasonryItem<CardData>{1, {u8"第一项", 80}},
                ryn::MasonryItem<CardData>{2, {u8"第二项", 120}}}};
ryn::Masonry(ryn::MasonryProps<CardData>{}
    .items(cards)
    .columns(ryn::Responsive<int>{1}.at(ryn::Breakpoint::Md, 3))
    .gutter(ryn::dp(16), ryn::dp(12))
    .itemRender([](ryn::MasonryItemView<CardData> item) {
        ryn::Card(ryn::CardProps{}.title(ryn::bind([item] { return item.value().title; })),
                  ryn::CardContent{[] { ryn::Input(ryn::InputProps{}); }});
    }));
```

`MasonryItem<Data>(key, data)` 保存不可变 typed 数据；`.data(newData)` 生成该项的新数据值，items 更新时通过 retained binding 传给原内容，不重跑 itemRender。`MasonryItemView` 暴露 `key()`、`value()`、reactive `data()`、`index()`、`column()`。`.children(MasonryContent{...})` 优先于 itemRender；显式替换 children slot 只替换该键的内容，保留键的布局 owner。仅重排、修改数据/高度/列、调整窗口时保持原内容和编辑状态；移除键销毁其内容，新键仅挂载一次。

`columns` 默认为 3，接受正整数或 `Responsive<int>`。`MasonryGutter` 的 horizontal/vertical 分别支持 `Responsive<LogicalLength>`；单参数 `.gutter(dp(...))` 同时设置双轴。断点与 Grid 共享 `<ryn/breakpoints.hpp>`，按窗口可用逻辑宽度解析，默认 xs=480、sm=576、md=768、lg=992、xl=1200、xxl=1600、xxxl=1920；xs 为窄窗口的默认覆盖。`.breakpoints(Prop<Breakpoints>)` 可调整阈值，嵌套容器宽度只决定实际列宽。

排版按 items 顺序，使用实际宽度测量每项，再选择当前最短列，等高时选择较小的零基列索引。`.column(n)` 显式指定非负列；参照上游，列数缩小时将较大的指定列限制到最后一列。`.height(dp(h))` 强制该项的逻辑高度，省略时使用真实测量高度；高度为零有效。总高度不含最后一个垂直 gutter。窄窗口中水平 gutter 按可用宽度压缩，确保列宽和坐标非负、有限；RTL 只映射物理位置。

原生 reactive 尺寸失效会自然重新测量，不依赖浏览器 ResizeObserver。`.fresh(true)` 在宿主每次同步时主动重新测量，也可用 `.refresh(Prop<std::uint64_t>)` 显式刷新外部尺寸源；组件不为监测创建持续帧请求。measure/place 共用 LayoutEngine 同一代结果，不在放置阶段重挂载或重测内容。

`.onLayoutChange` 返回 `{key, column}` 数组，仅实际分配或顺序变化时通知，不因坐标或高度单独改变重复发送。回调在放置完成后的安全同步点触发，允许请求下一次布局、修改 items 或销毁 owner。索引和实际列 binding 也保留到安全同步点更新；不会在测量遍历中调用业务布局变化回调。

列数为零/负数、负列号、负/auto/NaN/Infinity 高度、负/非有限 gutter 显式拒绝；无效响应项即使尚未匹配也拒绝。无 children 且无 itemRender 的项目保留为空内容，可用显式高度预留空间。超出有限逻辑坐标的实际布局拒绝发布，不生成非有限几何。新增内容抛错时回滚新增分支，保留原键，允许调用方修复后在后续同步重试。

基线为 [Ant Design 6.6.5 Masonry](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/masonry/index.en-US.md) 及其 [列分配实现](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/masonry/hooks/usePositions.ts)。DOM、CSS semantic styles/classNames 和 React 节点接口不迁入原生 API；内容视觉由现有 Theme/Component Token 控制。Masonry 不直接生成 GPU 数据或依赖 SDL。
