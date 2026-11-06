# Listy

107 本批 Listy 已通过 Windows/MSVC HEADLESS Debug/Release 定向合同，目录为 partial；隐藏 GPU 图像与窗口集成尚未完成，tasks 36.1/36.2 保持未勾选。

```cpp
ryn::Signal<std::vector<ryn::ListyItem<ryn::String>>> accounts{
    std::vector<ryn::ListyItem<ryn::String>>{{u8"alice", u8"Alice"}, {u8"bob", u8"Bob"}}};
ryn::Listy(ryn::ListyProps<ryn::String>{}
    .items(accounts).columns(2).height(240.0F).pagination().pageSize(10)
    .itemRender([](ryn::ListyItemView<ryn::String> item) {
        ryn::Text(ryn::TextProps{}.content(item.data()));
    }));
```

每项必须提供唯一非空 String key 与 owned typed 数据。`ListyItem<T>::data` 更新数据；同 key 的 `itemRender` 只执行一次，内部控件使用 `view.data()` / `view.index()` 读取 Binding。删除销毁该行，重排更新行顺序和数据，未变 key 的编辑器与草稿保留。空/重复 key 在变更 live UI 前拒绝；renderer 或数据观察者异常退役整个 Listy，清理部分挂载、输入、场景及 ref。

默认纵向、单列；`columns(Prop<size_t>)` 提供等宽网格，按行取最大实际高度；`direction(Horizontal)` 使用横向实际测量与滚动，`rtl` 控制横向起点及网格列顺序。`height` 为有限非负逻辑 dp，限定视口高度。内容按实际尺寸滚动，不依赖固定行高，也不宣称虚拟化。行留白取现有 Theme map.size_small，文字继承当前 Text 语义，空状态与分页复用各自 Theme；具体行视觉由 typed slot 的组件 Theme 决定，不通过 LayoutStyle 注入视觉。

`pagination`、`current` / `defaultCurrent` 和 `pageSize` 使用真实 Pagination。页码从 1 开始，total 收缩时仅规范化当前显示页；受控 current 的应用值不被改写。`onChange(page,size)` 是请求，应用可接受或拒绝。未显示页保留行子树并休眠输入，返回恢复原草稿。此版本仍保留所有记录，不承诺大数据虚拟化。

`ListyRef::scrollTo(offset)`、`scrollTo(key, Start/End/Auto, offset)` 控制当前显示页内的滚动。offset 使用逻辑 dp，非法数值拒绝，移除/离页后的 ref 返回 false。滚轮归实际最上层命中所在的最近 Listy，防止覆盖浮层或嵌套列表穿透；到达边界时允许外层继续滚动。`onScroll` 通知实际偏移。`header` / `footer` / `emptyContent` 为独立 typed slots。

`visible(false)` 休眠内容，`disabled(true)` 保留画面并阻断后代输入，清理焦点、捕获、编辑会话与子浮层。普通数据、布局及状态更新不重建相同 key 的 item slot。

锁定参考为 Ant Design 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Listy index、API 与 basic/rich/scroll-to demos。方向/网格/分页为已批准 RynUI 原生矩阵；上游 group/sticky/virtual 并非本批矩阵目标，尚未提供。DOM/CSS/React ref、浏览器 UIEvent 不移植。

独立页 `gallery.sample.listy.retained` 提供编辑、重排、网格、分页、连续滚动、方向、禁用和空态恢复。已实际通过 MSVC HEADLESS Debug/Release 的 `rynui_portable_listy_component` / `rynui.portable.listy_component`，以及 Windows hidden D3D12 的草稿保留、重排、滚动、方向和空态操作。viewport、header、分页与 footer 的非空区域按 Theme map.size_small 分隔，分页不紧贴裁剪的行。结果见 [107 集成](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/display-windows-integration.md)；Linux 原生结果仍独立待验。
