# Splitter 分割面板

基线：[Ant Design 6.6.5 Splitter](https://github.com/ant-design/ant-design/blob/6.6.5/components/splitter/index.en-US.md)。公开入口为 `Splitter(SplitterProps, std::vector<SplitterPanel>)`，每个面板通过 `SplitterPanelContent` 提供保留内容；支持单面板、多面板和任意嵌套。

```cpp
Splitter(SplitterProps{}.lazy().layout(LayoutStyle{}.height(dp(300))), {
    SplitterPanel{SplitterPanelContent{[] { Input(InputProps{}); }}}
        .defaultSize(SplitterSize::percent(30))
        .min(SplitterSize{dp(100)})
        .collapsible(),
    SplitterPanel{SplitterPanelContent{[] { Text(u8"正文"); }}}
        .min(SplitterSize::percent(20))
});
```

`SplitterSize{dp(value)}` 表示逻辑长度，`SplitterSize::percent(value)` 表示 0–100 的百分比，无 CSS 字符串。`size(Prop<SplitterSize>)` 受控，`defaultSize` 是非受控初值，两者互斥。未指定初值的面板均分剩余空间；容器变化时按初值的逻辑/百分比单位重新解析；交互提交整组逻辑尺寸后，再随容器按比例归一化。部分受控时自动面板吸收受控值的空间差；存在任一受控 size 时，交互仅发出整组尺寸请求，由调用方回写。百分比始终根据当前分割轴的可用逻辑尺寸解析。

`orientation(Horizontal|Vertical)` 优先于 `vertical` 简写；`direction(RightToLeft)` 只反转水平次序与箭头。分隔条覆盖接缝，不从面板总尺寸扣除额外间距。尺寸求解保证有限、非负；相邻拖动只调整相邻两个面板，保持它们总尺寸和其他面板尺寸。多个分隔条因零尺寸面板重合时，首次拖动方向选择对应的相邻有效面板对，允许向前侧恢复中间隐藏面板。`min/max` 支持两种 typed 单位。正常可满足约束时全部遵守；小容器无法容纳 minimum 时按比例收缩下限，maximum 总和无法覆盖容器时放宽可见面板的上限，总尺寸和非负性优先。显式折叠允许目标为 0，空间转移给相邻面板；恢复重新应用当前 min/max。所有面板显式为 0 时归一化为均分，避免无可展开的空布局。

分隔条支持 PointerRouter 捕获、多指针身份隔离、Tab、方向箭头、Home/End；默认键盘步长 10dp，Shift 为五倍。Home/End 使用相邻约束可达边界。`resizable(false)` 禁止相邻分隔条拖动与键盘调整，折叠配置保持独立。窗口失活、输入边界变化、Escape、指针取消、约束/方向/lazy 改变或销毁结束捕获；取消不补发提交事件，持续模式已提交的有效尺寸保留，lazy 候选丢弃。

`lazy(true)` 拖动期间只更新 logical scene 的真实预览线，内容 bounds 和布局尺寸保持原值；释放时提交并发出一次 onResize，再发出 onResizeEnd。持续模式每次尺寸变化发出 onResize。指针和键盘开始调整时发出 onResizeStart，正常结束时发出 onResizeEnd；事件值始终为整组逻辑尺寸，受控拒绝不改变实际尺寸。`onDraggerDoubleClick(index)` 单独通知双击，不自动重置状态，也不启动该次拖动。

`collapsible(true)` 启用面板两侧折叠能力；细粒度 `SplitterCollapsible{start, end, show_icon}` 对应面板前/后接缝。`show_icon` 支持 Auto（悬停或键盘聚焦显示）、Always、Hidden。每个接缝的两个按钮分别朝起始/结束方向折叠或恢复，恢复优先使用折叠前逻辑尺寸，再应用当前约束。折叠调用 onResize、onResizeEnd、onCollapse(collapsedFlags, sizes)。自定义图标使用 `SplitterDraggerIcon`、`SplitterStartIcon` 和 `SplitterEndIcon` typed slots，普通尺寸变化不重挂载图标。

默认保留隐藏内容及其草稿，隐藏分支不能命中或聚焦，捕获和文本会话被清理。`destroyOnHidden` 可在 Splitter 设置，也可在 Panel 覆盖；销毁后内容引用失效，恢复重新创建新 generation。内容挂载与销毁在安全同步点执行；窗口暂时为零尺寸时的重新挂载会请求后续帧，不在布局遍历中运行用户工厂。回调间重新核对 owner/generation，回调内销毁或抛错不会留下捕获或调用旧回调。

`collapseMotion(true)` 为显式折叠/恢复启用 Theme 的 slow/ease-in-out 动画。动画单独保持 visual sizes 和目标 sizes；隐藏输入按目标状态立即停用。reduced motion 或 Theme 关闭 motion 时直接提交，结束后不持续请求下一帧。

视觉只通过 `ThemeConfig.splitter.tokens` 设置：面板背景、分隔线/悬停/激活、图标、折叠按钮、焦点、预览色，以及 splitBarSize、splitTriggerSize、splitBarDraggableSize 对应的 typed 指标和键盘步长。所有生产内容只写 logical CPU scene，不依赖 renderer 或 SDL。Gallery 独立页包含多面板约束、受控百分比、禁用与双击重置、自定义图标、lazy、双侧折叠、动画、全局/面板隐藏策略及嵌套水平/垂直示例。Web DOM/CSS/React 与弃用别名不作为原生 API。
