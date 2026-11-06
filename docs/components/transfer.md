# Transfer

`<ryn/transfer.hpp>` 提供 `Transfer(TransferProps)`。合同固定参照 Ant Design 6.6.5、commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 API、类型、Section/ListBody/Actions、样式和公开 demos；参照源码副本及逐文件哈希属于忽略的本地 `out/` 产物，不随仓库分发。

## 数据与受控请求

`TransferKey = variant<String, double>`；key 必须非空、唯一且数值有限。`dataSource(Prop<vector<TransferItem>>)` 按 key 保留行、Checkbox、Input/TextArea 与 `TransferItemContent`。`rowKey` 可从 typed item 生成 key。左右移动和重排保留富内容草稿；移除、隐藏、分页或移动会结束旧分支的 capture、焦点和 preedit。

`targetKeys/defaultTargetKeys` 管理目标集合，`selectedKeys/defaultSelectedKeys` 管理两侧已选项；受控和 default 配置不能同时使用。RynUI 保留原有 uncontrolled 扩展。受控模式只发送请求，应用可接受、拒绝或同步重入更新。`onChange(TransferChangeInfo)` 返回完整 target keys、语义方向与 moved keys，`onSelectChange(TransferSelectInfo)` 分别返回 source/target selection。非法/重复请求在修改前拒绝，未知 key 不产生部分选择；保留列表 callback 每次核对 key 的当前侧、disabled、线程和 generation。

## 列表、搜索、分页与方向

两侧组合真实 Checkbox、Input、Button 和 Pagination；提供全选/取消、当前页选择、当前页反选、清空，以及 oneWay 的单行/当前页/全部移除。禁用数据不参与这些操作。Checkbox 键盘操作支持 Space，Shift+Click / Shift+Space 使用同侧稳定 key 锚点作范围选择；输入控件的 composition、Enter、Tab/Shift+Tab 和 innermost-popup Escape 由既有 Core 输入服务处理。

`showSearch(bool)` 或 `showSearch(TransferSearchOptions)` 配置搜索可见性、placeholder 和每侧初始值；另保留 `searchPlaceholder` 与 `searchDefaultValue`。`filterOption`、`render` 和 `onSearch` 按侧处理 UTF-8 文本；过滤不改变完整数据和应用受控权威。`notFoundContent` 可覆盖 locale 的空状态。

`TransferPagination` 字段为 `enabled/page_size/show_size_changer/simple/show_less_items/page_size_options/controlled_page_size`。两侧独立 page/page size，页大小通过真实 Pagination Select 修改；原有默认页大小模式保持兼容，`controlled_page_size=true` 表达上游显式 pageSize 的受控权威，`onPageChange(TransferPageInfo)` 可接受或拒绝请求；总数收缩、过滤与尺寸变化会 clamp 页码和滚动。`renderList` 按上游合同停用内置分页，并接收完整的 filtered items。边界 wheel 交还外层，焦点进入列表行时自动 reveal；普通 idle/no-op 不维持帧循环。

`listWidth/listHeight/rowSpacing/size/pagination/showSearch/direction` 接受响应属性，先校验最终有限几何；非法输入回滚既有结构或完整 retire 此 Transfer generation，不提交半更新场景。`TextDirection` 显式映射为 `CheckboxDirection`；RTL 同时改变两侧排列、Checkbox、移除位置、动作箭头、搜索、分页和 footer 分配。

## Locale、保留 slots 与 Theme

`locale(Prop<TransferLocale>)` 包含两侧标题/空状态、搜索、单复数单位、加载、全选/取消、当前页、反选、清空、移除、方向动作及分页文案。`titles`、`operations/actions` 文本和 `selectAllLabels` 也可响应更新。`title/footer/TransferItem.content`、`renderList`、`selectAllLabel`、`selectionsIcon` 与 `action` 是 typed retained slots；普通数据、Theme、locale、几何或控制状态变化不会重新执行 slot factory。

`TransferListContext` 包含 `direction/filtered_items/selected_keys/disabled/query/locale/text_direction` 和 `on_item_select/on_item_select_all/on_item_remove`，以及响应式 `show_remove`；`footer(factory)` 使用同一上下文。自定义列表自行实现列表布局/滚动，保留 child Input、Table、Tree 或其他原生组合。`TransferActionContext` 提供语义方向、当前 disabled、label、text_direction 和经过当前状态校验的 `activate`；`actions` 数组按向右、向左排列，`additionalActions` 挂载额外 typed 内容。窗口销毁、卸载、跨侧更新或外线程之后的旧 callback 不修改 UI。

`ThemeConfig::transfer` 独立提供 typed `TransferTokenOverride`、component seed 和 algorithm，支持作用域继承及 `inherit=false`。`ThemeSnapshot::transfer()` 分为 17 色 palette、13 项 metrics、typography、effects 与 order；六个上游原生 Token 为 listWidth/listWidthLG/listHeight/itemHeight/itemPaddingBlock/headerHeight，其余为原生布局/效果的 typed 映射。背景、边框、header/list/search/row/actions/footer、disabled、error/warning、焦点效果与层序均实际消费 Token。动作列根据保留内容的自然宽度布局，`actions_width` 为最小宽度。颜色只触发 paint/material，metrics/typography 触发相关布局，Theme/locale 更新保留行和无关组件身份。所有场景更新走 logical CPU scene 和共同 retained surfaces；`LayoutStyle` 只控制外部布局。

`disabled/readOnly/loading/visible`、modal input boundary、窗口失活及零面积会停止失效分支输入与文本会话。绑定、过滤、render、factory 或事件回调抛异常时，组件会在发布前回滚或完整 retire，释放其 popup、focus、capture、文本会话和场景资源；其他组件继续存活。

未设置 `locale` 时实时继承最近 ConfigProvider 的 Transfer 组，内部 Pagination 自行继承同一作用域，不传默认英文 aggregate；显式完整 TransferLocale 则将其 `pagination` 继续传给内部分页。具体标题/空态/搜索/操作文字和 typed slots 优先，显式英文与空串不会退回默认值。`TransferSearchOptions::placeholder` 改为 `optional<String>`：缺省继承，显式空值清空 placeholder；直接读取该字段的调用者需先判断/解引用。

语言投影使用已有过滤结果与选择计数，只更新保留文字，不运行 render/filter/reconcile 或工厂，不改变 page、scroll、selection 或输入会话。继承包使用 `count_one/count_other` 模板；显式旧 Locale 保留 `item_unit/items_unit` 的 `selected / total unit` 合同；`selectAllLabels` 结果仍优先且不会因语言变化重算。两条内置计数路径均使用现有 i18n UInt 精确插值，当前数字为 ASCII 十进制，不引入 number helper。新增组合测试源码见 `rynui.i18n_components_collections`，本次未运行 build/CTest。

## 平台边界

平台通用合同和实际平台结果分别记录于 change 107 的 `evidence/transfer-linux.md` 与 `evidence/transfer-integration.md`。Core 文案、逻辑输入和 composition 合同不代表 OS 屏幕阅读器或系统 IME 候选窗实测。DOM/CSS/React、HTML 属性、浏览器事件及 Web refs 不属于公开原生接口；Windows MSVC 和物理系统 IME/AT 由对应平台单独验证，不列人工实施任务。
