# Pagination

原生分页使用 `PaginationProps`，可与 Table 或普通列表共享受控页码：

Token Gallery 的 LiveSamples 末尾将 Pagination 与实际可编辑 Table 共享页码，每页四条记录。翻页保留已有行的编辑值与选择；数据恢复同时重置页码和排序。

```cpp
ryn::Signal<std::size_t> page{1};
ryn::Pagination(ryn::PaginationProps{}
    .total(std::size_t{125})
    .current(page)
    .pageSize(std::size_t{10})
    .onChange([page](std::size_t next, std::size_t) { page.set(next); }));
```

省略 current 时采用 defaultCurrent（默认 1）；两者不能同时设置。pageSize 必须大于零，默认 10。total 为零时仍显示第 1 页；hideOnSinglePage 可隐藏整组内容。总数收缩时显示页夹紧到有效范围，不自动回调；受控应用应同时规范自己的 page 状态。外部随后改变 total 不恢复先前越界页码。

前后页、首尾页、当前页附近至多五页与跳转省略按钮使用最多 11 个保留按钮。省略按钮跳转五页，Tab/Shift+Tab、Enter/Space 沿用 Button。相同页、disabled 和边界操作不回调。pageSize 可受控，也可由 defaultPageSize 初始化后内部管理；扩展见下节。

视觉基线为 Ant Design 6.6.5，Default/Dark/Compact 跟随 Theme。`ThemeConfig::pagination.tokens` 提供 item/active/disabled、背景、border、item_size/small/large、gap、border_radius；按钮的焦点和 hover 继承共同 Button 主题。LayoutStyle 只控制外部大小。有限外宽小于分页内容时裁剪；可启用 responsive 或分配足够宽度。

历史首版使用 MSVC headless 验证交互和生命周期；本轮 Linux 结果见下节及 change 107 evidence。HEADLESS 不代表原生 GPU 或系统 IME 验收。

## Change 107 原生扩展

`defaultPageSize` 提供非受控页大小；`pageSize` 和 `defaultPageSize` 互斥。`showSizeChanger` 启用原生 Select，未指定时 total > 50 自动显示；`pageSizeOptions` 接受响应式正整数列表，当前页大小始终保留为选项。用户选择后先请求 `onShowSizeChange(page,size)`，再请求 `onChange(page,size)`；受控 page/pageSize 分别等待应用回写。回调中销毁宿主后不继续发送事件。

`showQuickJumper` 和 `simple` 使用保留的原生 Input，Enter 或失焦提交；空串、负数、小数、非数字和溢出整数不发请求，零与越界整数夹紧到有效页。simple 的 `readOnly` 禁止编辑，前后页仍可用。total 收缩不发事件。total=0 的范围为 `[0,0]`。

`responsive` 根据 106 共享响应式宿主的窗口宽度在 576 logical px 以下使用 Small 并隐藏独立快速跳页；simple 页码编辑器保留。`align(Start|Center|End)` 控制可用行宽内的位置。`showTotal` 接受 `(total, PaginationRange)` 格式化函数。`itemRender` 是一次挂载的 typed 内容工厂，接收只读响应式 Binding page/type/label/active，可在 bind 中读取并派生内容；工厂完整接管按钮内部内容，包括前后页与省略项，不再叠加默认图标；内部 Prop 更新保留内容身份。按钮输入、禁用和焦点仍由 Pagination 管理。

Gallery 独立页提供页大小、跳页、总数、simple 和 typed renderer 实例。上述扩展基于 [Ant Design 6.6.5 固定源码](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/pagination/index.en-US.md)。Linux HEADLESS 是逻辑/输入合同证据；实际 GPU、系统 IME 与 Windows 平台结果由分类集成单独记录。
