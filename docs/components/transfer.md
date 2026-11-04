# Transfer

`<ryn/transfer.hpp>` 提供 `Transfer(TransferProps)`。`TransferKey` 为 `variant<String, double>`，每条数据具有非空、唯一、有限的 key。重复/非法 key 在结构更新前拒绝。`dataSource(Prop<vector<TransferItem>>)` 按 key 保留 Checkbox、行内 Input 和 typed `TransferItemContent`，左右移动只改变行位置。

`targetKeys/defaultTargetKeys` 管理右侧集合，`selectedKeys/defaultSelectedKeys` 管理左右已选项。受控请求不会擅自覆盖应用状态。`onChange` 返回完整 target keys、移动方向和本次 moved keys；`onSelectChange` 分别返回 source/target selected keys。禁用项不被全选、反选或移动修改。

两侧使用真实 Checkbox、Input、Button 和 Pagination。`showSearch`、`filterOption`、`onSearch` 提供按侧搜索；`pagination({enabled,page_size,show_size_changer})` 提供每侧独立分页与 10/20/50 页大小操作。`oneWay` 隐藏向左移动操作，改为右侧逐项删除。`showSelectAll`、反选/清空按钮、精确滚轮、`onScroll` 与 `listHeight/listWidth` 形成列表交互。

`title`、`footer` 使用保留 typed slots；`render` 定制普通文本，`TransferItem.content` 定制保留富内容。`renderList` 每侧挂载一次，接收响应式 `filtered_items/selected_keys/disabled` 及选择 callbacks。callback 绑定挂载线程与窗口生命周期，外线程、跨侧请求和销毁后的旧 callback 不修改 UI。自定义列表自行实现其布局和滚动；遵循上游约定，不叠加内置分页。

`size/status/direction` 传递至组合控件，`disabled/readOnly/visible/loading` 阻止失效分支消费输入。所有视觉继承原有 Input/Checkbox/Button/Pagination Theme，`LayoutStyle` 只控制外部布局。

此阶段支持状态为 partial：独立 Transfer Component Token、完整可配置本地化/动作 slots、列表几何 Prop 的完整动态更新与非法值回滚，以及 Checkbox 方向细节和复杂自定义列表/IME/嵌套浮层的完整自动合同仍需补齐。Linux GPU、Windows MSVC、系统 IME 和辅助技术未由本次 HEADLESS 结果证明。
