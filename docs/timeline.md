# Timeline

原生 vertical 时间轴，参考本地锁定 Ant Design 6.6.5（4a39f54842eade4e565ab336ef6097cd7e723cdd）的 Timeline.tsx、useItems.tsx 与 style/index.ts。此文档描述 087 合同，验收结果见该 change 的 evidence。

`Timeline(TimelineProps, vector<TimelineItem>)` 固定 keyed topology，key 必须非空且唯一。`mode` 支持 Start（默认）/End/Alternate，item `placement` 可覆盖；Alternate 按源索引分侧后再 reverse。隐藏不改变其他 item 的源索引。`reverse` 同步视觉与子控件 Tab 顺序。`visible` 支持 root 与单个 item，空/全隐藏退出 flow。

item 的现代字段是 `title`、`content`、`icon`、`loading`、`placement`。title/content 接受 `Prop<String>`；`richTitle(TimelineTitle)`、`richContent(TimelineContent)`、`icon(TimelineIcon)` 只挂载一次，富 slot 优先于对应文本。title 非空或存在富 title 时采用双侧布局；Start 时标题在左、正文在右，End 相反。无 title 时同侧排列，Alternate 始终双侧。`loading` 使用静态 loading 图标和虚线轨迹，不旋转、不持续调度帧；自定义 icon 优先。

`tone` 为 Default/Blue/Red/Green/Gray，默认节点和 Blue 均映射 colorPrimary。视觉只通过 `ThemeConfig::timeline`：colors 为 text/tail/dot_background/node/blue/red/green/gray；metrics 为 tail_width/dot_border_width/dot_size/icon_size/item_padding_bottom/content_gap/font_size/line_height/font_family。支持全局及组件算法、继承、inherit=false 与 typed override。最终 metric 必须有限、非负且组合不过溢，font_size/icon_size>=.5、line_height>0；非法后代组合拒绝整次更新。

`LayoutStyle` 只控制外部布局。首版不提供 horizontal、titleSpan、RTL、动画、Web AT、动态增删/任意排序或任意 style，不提供 deprecated pending/pendingDot/children/dot/label/position。headless 测试不代表 GPU/native/Linux/系统 IME/AT 验收。

可编译示例见 [examples/timeline_example.hpp](../examples/timeline_example.hpp)，由 `rynui.timeline_example` 在真实组件 host 中挂载并更新。最小使用方式（在 Content scope 内）：

```cpp
ryn::Timeline(ryn::TimelineProps{}.mode(ryn::TimelineMode::Alternate), {
    ryn::TimelineItem{u8"created", u8"Request created"}.title(u8"09:00").tone(ryn::TimelineTone::Green),
    ryn::TimelineItem{u8"pending", u8"Waiting"}.title(u8"09:30").loading(true)
});
```

每个 slot 保留自然内容尺寸。超出分配宽/高的文本、图标与控件内容被独立裁剪，不通过缩小 Button/Input/Select 避免越界；文本不会自动换行。focus 外扩使用既有 decoration clip，仍服从 Timeline 及祖先 hardclip。隐藏会清理焦点、preedit、capture 和 popup；恢复保留草稿，不复活此前被强制 suppression 的 popup。
