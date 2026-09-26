# Proposal

## Why

Token Gallery 当前用一组横向换行的小按钮表示导航，和锁定的 Ant Design 6 组件文档的顶部站点栏、左侧分类目录与右侧正文结构不一致。Gallery 的文档滚动还平移包含导航与正文的共同根节点，导致左栏跟着右侧正文一起移动；目前也没有可见滚动条。现有公开布局没有 ScrollArea/Clip 合同；本次先在 Gallery 中建立可验证的滚动区与滚动条，再判断公开组件所需的通用裁剪边界。

## What Changes

- 以 Ant Design 6 官方组件页为参考，建立固定顶部站点栏，把桌面宽度下的左栏整理为纵向的文档入口、组件类别和组件条目；右侧保留现有离线目录、状态筛选和真实样例。
- 先完成 Gallery 内部滚动区及可见滚动条：左右列各有轨道与滑块，支持滚轮、拖动滑块及点击轨道；正文滚动不再改变左栏位置。窄窗口回到单列文档流并保留滚动条。
- 保持 section/category 导航、稳定挂载和已有输入验收；增加布局、滚动隔离及真实 Windows D3D12 证据。
- 根据用户反馈修复持续拖动中段的崩溃，并进一步统一导航对齐、正文宽度、标题层级和留白；补充连续往返拖动与宽窄窗口排版验收。

## Capabilities

### New Capabilities

- `gallery-independent-scroll-regions`: Gallery 两列独立滚动、可见滚动条及指针路由。
- `gallery-document-sidebar`: 与锁定组件目录一致的纵向分组导航。

### Modified Capabilities

无。

## Impact

涉及 Gallery 自身的 viewport、装饰组件、布局和窗口事件，以及相应测试；不增加不完整的公开 ScrollArea API。正文和导航以顶栏下方的固定区间为可视范围，并由顶栏覆盖越界绘制；本次不声称任意嵌套区域已具备通用裁剪。正式 Windows MSVC/D3D12 验收记录布局、滚动条交互、终态与性能变化，不把 CPU 帧时间当 GPU 时间。
