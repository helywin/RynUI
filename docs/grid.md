# Grid Row/Col 原生栅格

公开入口为 `<ryn/grid.hpp>` 的 `Row(RowProps, RowContent)` 与 `Col(ColProps, ColContent)`。Row 只接受直属 Col；Col 内可以组合任意原生内容。所有配置支持 reactive `Prop<T>`，重排不重跑内容或重建编辑器。

```cpp
ryn::Row(ryn::RowProps{}.gutter(ryn::dp(16), ryn::dp(12)), ryn::RowContent{[] {
    const auto sizes = ryn::Responsive<ryn::ColOptions>{ryn::ColOptions{.span = 24}}
                           .at(ryn::Breakpoint::Md, ryn::ColOptions{.span = 12})
                           .at(ryn::Breakpoint::Lg, ryn::ColOptions{.order = 1});
    ryn::Col(ryn::ColProps{}.responsive(sizes), ryn::ColContent{[] {
        ryn::Input(ryn::InputProps{}.defaultValue(u8"调整窗口保留草稿"));
    }});
}});
```

`span`、`offset`、`push`、`pull` 接受 0..24；`order` 为整数，只改变视觉布局次序，声明、绘制和键盘顺序保持。跨度加偏移超过一行时整列换行，`wrap(false)` 保留单行。push/pull 是相对逻辑位置，不占用额外排版空间，RTL 同时映射偏移和位移。span=0 保留内容身份并立即阻断输入、焦点和捕获，恢复后继续使用原状态。

`RowAlign` 提供 Top/Middle/Bottom/Stretch；Stretch 保留 Col 的外部 min/max height，显式高度不拉伸。`RowJustify` 提供 Start/End/Center/SpaceAround/SpaceBetween/SpaceEvenly。align/justify 可用 `Responsive<T>` 逐断点配置。`GridGutter` 的 horizontal/vertical 分别是响应式逻辑长度；水平使用每列半间距 padding 和 Row 的半间距外延，垂直只在行之间留空。LayoutStyle 仅控制外部尺寸、约束和 margin，不控制视觉。

`GridFlex` 用 typed grow/shrink/logical basis 替代 CSS 字符串：`GridFlex(n)` 对应 n/n/auto，`fill(n)` 对应 n/1/0，`fixed(dp(...))`、`automatic()`、`none()` 以及三参数构造覆盖固定、弹性和自动基准。flex 优先于非零 span 的宽度；min/max 约束冻结后继续分配剩余空间。无 span/flex 的列使用内容自动尺寸。

共享 `<ryn/breakpoints.hpp>` 的 `Responsive<T>` 默认值先应用，再按 xs→xxxl 顺序应用匹配项。ColOptions **逐字段**叠加，所以 lg 只改 order 时继续保留 md 的 span/offset；缩小时重新从基值解析。xs 是默认覆盖，包含小于 480 的窗口；默认屏幕 token 为 xs=480、sm=576、md=768、lg=992、xl=1200、xxl=1600、xxxl=1920。`Breakpoints` 允许显式配置严格递增的有限非负阈值，Row 通过 `.breakpoints(Prop<Breakpoints>)` 指定。

断点取 WindowComponentServices 在每帧测量前发布的**窗口可用逻辑宽度**，不取嵌套容器宽度。内部共享接口是 `services.responsive_layout().window_width()`；接入其他原生框架时同样应在测量前发布窗口宽度。measure/place 使用 LayoutEngine 同一代数据，无布局动画或常驻轮询。

基线为 [Ant Design 6.6.5 Grid](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/grid/index.en-US.md)。DOM、React hooks、CSS 单位字符串属于 Web 接口，不迁入原生 API。布局组件不产生私有 renderer/SDL 上传或视觉样式。
