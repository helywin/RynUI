# Pagination

原生分页使用 `PaginationProps`，可与 Table 或普通列表共享受控页码：

```cpp
ryn::Signal<std::size_t> page{1};
ryn::Pagination(ryn::PaginationProps{}
    .total(std::size_t{125})
    .current(page)
    .pageSize(std::size_t{10})
    .onChange([page](std::size_t next, std::size_t) { page.set(next); }));
```

省略 current 时采用 defaultCurrent（默认 1）；两者不能同时设置。pageSize 必须大于零，默认 10。total 为零时仍显示第 1 页；hideOnSinglePage 可隐藏整组内容。总数收缩时显示页夹紧到有效范围，不自动回调；受控应用应同时规范自己的 page 状态。外部随后改变 total 不恢复先前越界页码。

前后页、首尾页、当前页附近至多五页与跳转省略按钮使用最多 11 个保留按钮。省略按钮跳转五页，Tab/Shift+Tab、Enter/Space 沿用 Button。相同页、disabled 和边界操作不回调。pageSize 由应用控制；首版没有内置 size changer、quick jumper、total slot 或 responsive 自动简化。

视觉基线为 Ant Design 6.6.5，Default/Dark/Compact 跟随 Theme。`ThemeConfig::pagination.tokens` 提供 item/active/disabled、背景、border、item_size/small/large、gap、border_radius；按钮的焦点和 hover 继承共同 Button 主题。LayoutStyle 只控制外部大小。有限外宽小于分页内容时裁剪；应用可选 Small 或分配足够宽度。

本次使用 MSVC headless 验证交互和生命周期；不代表原生 GPU、Linux 或人工视觉验收。
