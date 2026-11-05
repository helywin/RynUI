# Timeline

原生横纵时间轴，参考本地锁定 Ant Design 6.6.5（4a39f54842eade4e565ab336ef6097cd7e723cdd）的 Timeline.tsx、useItems.tsx 与 style/index.ts。此文档描述 087 基础及 107 扩展合同，验收结果见该 change 的 evidence。

`Timeline(TimelineProps, vector<TimelineItem>)` 保留式 keyed topology，key 必须非空且唯一。`mode` 支持 Start（默认）/End/Alternate，item `placement` 可覆盖；Alternate 按源索引分侧后再 reverse。隐藏不改变其他 item 的源索引。`reverse` 同步视觉与子控件 Tab 顺序。`visible` 支持 root 与单个 item，空/全隐藏退出 flow。

item 的现代字段是 `title`、`content`、`icon`、`loading`、`placement`。title/content 接受 `Prop<String>`；`richTitle(TimelineTitle)`、`richContent(TimelineContent)`、`icon(TimelineIcon)` 只挂载一次，富 slot 优先于对应文本。title 非空或存在富 title 时采用双侧布局；Start 时标题在左、正文在右，End 相反。无 title 时同侧排列，Alternate 始终双侧。`loading` 使用现有 LoadingOutlined Icon spin 与 motion policy，未显示、离页、window inactive 或 reduced motion 时不保持活跃旋转；自定义 icon 优先。

正常顺序中，通向 loading 节点的前一段 rail 使用虚线；反序后，该 loading 节点向下一显示项的 rail 使用虚线。最后显示项没有 tail。较长的可见虚线按既有 retained content range 容量分批发布，裁剪、反序和显隐不缩短轨迹。

`tone` 为 Default/Blue/Red/Green/Gray，默认节点和 Blue 均映射 colorPrimary。视觉只通过 `ThemeConfig::timeline`：colors 为 text/tail/dot_background/node/blue/red/green/gray；metrics 为 tail_width/dot_border_width/dot_size/icon_size/item_padding_bottom/content_gap/font_size/line_height/font_family。支持全局及组件算法、继承、inherit=false 与 typed override。最终 metric 必须有限、非负且组合不过溢，font_size/icon_size>=.5、line_height>0；非法后代组合拒绝整次更新。

`LayoutStyle` 只控制外部布局。`orientation(Horizontal/Vertical)` 计算真实横纵 frame 和 rail；Start/End/Alternate 指定正文位于 rail 的哪一侧。`rtl` 镜像实际几何，横向轨迹从当前节点裁剪连接到下一节点，最后可见项没有 tail。`variant(Outlined/Filled)` 通过既有 Theme 颜色显示轮廓/实心节点；`color(Prop<Color>)` 是 typed item 语义色，自定义 icon 仍自行声明内容。

`titleSpan` 使用 `TimelineTitleSpan::fraction(.25F)`、`logicalWidth(180)` 或 `columns(18)` 表达上游百分比、px 与 24 栅格数值；有限宽度下裁剪到可用范围。水平布局在每个 item 的共同 rail 上下分配标题/正文，垂直布局以 titleSpan 分配侧栏。非有限、负数和越界比例拒绝更新。

`Timeline(props, Prop<vector<TimelineItem>>)` 支持稳定 key 的增删、重排和 reverse；`titleRevision/contentRevision/iconRevision` 明确替换单个工厂，未改 slot 继续保留 Input、ref 和草稿。工厂失败清理整个时间轴；非法集合在修改前拒绝。pending 按 6.6.5 推荐方式使用 `items` 中的 `loading` item，而非新增 deprecated pending/pendingDot API。原生 loading 行为使用已有 Icon motion，不创造 Web-only 转场或字符占位。Web/AT 平台桥、物理系统 IME 与 Windows 的实测边界独立记录。

可编译示例见 [examples/timeline_example.hpp](../examples/timeline_example.hpp)，由 `rynui.timeline_example` 在真实组件 host 中挂载并更新。最小使用方式（在 Content scope 内）：

```cpp
ryn::Timeline(ryn::TimelineProps{}.mode(ryn::TimelineMode::Alternate), {
    ryn::TimelineItem{u8"created", u8"Request created"}.title(u8"09:00").tone(ryn::TimelineTone::Green),
    ryn::TimelineItem{u8"pending", u8"Waiting"}.title(u8"09:30").loading(true)
});
```

每个 slot 保留自然内容尺寸。超出分配宽/高的文本、图标与控件内容被独立裁剪，不通过缩小 Button/Input/Select 避免越界；文本不会自动换行。focus 外扩使用既有 decoration clip，仍服从 Timeline 及祖先 hardclip。隐藏会清理焦点、preedit、capture 和 popup；恢复保留草稿，不复活此前被强制 suppression 的 popup。

## Gallery

Timeline 目录直接进入“项目处理记录”，可编辑并保存发布后的跟进事项，切换 Start/Alternate/End、正反序、loading 与备注显隐。“编辑备注”位于条目外，可恢复隐藏内容并调用原 InputRef；位置、顺序、Theme 和显隐更新保留控件与普通草稿。091 的 `--overlays-timeline-only` 覆盖真实目录、输入、可见性与有限空间裁剪，实际进展见 [091 tasks](../openspec/changes/091-20261026-add-overlay-and-timeline-gallery/tasks.md)。107 在保留这些示例的同时新增横向、动态 topology、titleSpan、状态色有限过渡和 loading 的交互示例，数据标明为合成示例。

```cpp
ryn::Timeline(ryn::TimelineProps{}.orientation(ryn::TimelineOrientation::Horizontal)
    .titleSpan(ryn::TimelineTitleSpan::columns(6)).variant(ryn::TimelineVariant::Filled).rtl(),
    {ryn::TimelineItem{u8"done", u8"完成"}.title(u8"09:00").tone(ryn::TimelineTone::Green),
     ryn::TimelineItem{u8"pending", u8"处理中"}.title(u8"09:30").loading()});
```

状态色、typed color、节点 Filled/Outlined 与 Theme 色更新使用现有有限 motion envelope 在实际节点/图标颜色之间过渡；中途更新从当前已绘制颜色继续。窗口失活、隐藏、不可见裁剪和 reduced motion 收口，不保留 idle 帧请求。
