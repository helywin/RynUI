# Table

`Table<Row>` 用固定的强类型列显示有限业务数据。Row 可为自己的可复制 C++ 业务类型，不需要继承框架类或转成字符串字段字典。

Token Gallery 的 LiveSamples 末尾提供 12 条业务记录：行内 InputNumber 编辑、受控排序/Checkbox 选择、四项分页，以及更新、重排、删除、恢复和 loading 操作。相同业务 key 保留编辑器身份；锁定行禁止编辑与选择。

```cpp
struct Order {
    ryn::String id;
    ryn::String customer;
    int amount;
    friend bool operator==(const Order&, const Order&) = default;
};

ryn::Signal<std::vector<Order>> orders{
    std::vector<Order>{{u8"o-1", u8"张三", 120}, {u8"o-2", u8"李四", 80}}};
ryn::Signal<ryn::TableSort> sort{ryn::TableSort{}};
ryn::Signal<std::vector<ryn::String>> selected{std::vector<ryn::String>{}};
ryn::Signal<std::size_t> page{1};

ryn::Table(ryn::TableProps<Order>{}
    .dataSource(orders)
    .rowKey([](const Order& order) { return order.id; })
    .columns({
        ryn::TableColumn<Order>{u8"customer", u8"客户", [](ryn::TableCell<Order> cell) {
            ryn::Text(ryn::TextProps{}.content(ryn::bind([cell] {
                return cell.value().customer;
            })));
        }}.width(200.0F),
        ryn::TableColumn<Order>{u8"amount", u8"金额", [](ryn::TableCell<Order> cell) {
            ryn::Text(ryn::TextProps{}.content(ryn::bind([cell] {
                return ryn::String::from_utf8(std::to_string(cell.value().amount)).value();
            })));
        }}.width(120.0F).align(ryn::TableAlign::Right)
            .sorter([](const Order& a, const Order& b) { return a.amount < b.amount; })
    })
    .sort(sort)
    .onSortChange([sort](ryn::TableSort value) { sort.set(std::move(value)); })
    .rowSelection({selected, [selected](auto keys) { selected.set(std::move(keys)); }, {}})
    .current(page).pageSize(std::size_t{10}).scrollY(200.0F).bordered());

ryn::Pagination(ryn::PaginationProps{}
    .total(ryn::bind([orders] { return orders.get().size(); }))
    .current(page).pageSize(std::size_t{10})
    .onChange([page](auto next, auto) { page.set(next); }));
```

业务页面通常将 Table 与 Pagination 放在竖直 Flex 中。省略 Table.current 时显示整个数据源；设置 current 后才应用分页。页码从 1 开始；current/pageSize 必须大于零，超出总页数的页码只在显示时夹紧。Table 不会隐式改写应用的页码；排序或筛选后需要回到第一页时，应用在自己的 callback 中同时更新 page。

rowKey 必须非空且唯一，禁止使用排序后会变化的数组下标。相同 key 更新会保留行、cell slot 和其中的 Input 等组件；删除 key 会销毁整行，重新插入视为新生命周期。每次数据源发布先检查全部 key，非法数据抛出 `invalid_argument`，保留先前结构。分页外的新行和隐藏空状态会在挂载子内容前停用，不允许其 autoFocus 抢走当前编辑焦点。列 key 同样要求非空且唯一，列 schema 在挂载时固定。

cell slot 只执行一次。`TableCell<Row>::value()` 是响应读取，应放在 `bind` 中；在挂载函数中直接复制字段只会取得当时的快照。`cell.row()` 可将整行作为只读 `Prop<Row>` 传给自己的业务组件。`cell.key()` 是稳定身份，操作回调需要最新数据时读取 cell.value()。每次数据发布会复制 Row 到不可变记录，再更新对应行的 Signal；不会重新执行保留的 cell 或外部 sibling。

列 `.title(Prop<String>)` 可响应更新，`.header(TableHeader{...})` 替换默认标题并继承表头语义字体/颜色，排序按钮仍由 Table 提供。列宽以 logical px 表示，必须为有限非负数；0 表示分配剩余空间，并受 `min_column_width` 最小值约束。显式列宽之和不足视口时右侧保留空白；超过视口时水平滚动。宽度和对齐可响应更新，每个 cell 裁剪自己的内容。Row 高度由内容与 token padding 测量，不强制单行文本。

排序为受控 `TableSort{column_key, None|Ascend|Descend}`，按钮循环升序、降序、无排序。`.sorter` 接受严格弱序 less 比较器，Table 按其稳定排序；仅 `.sortable()` 时 Table 发出排序请求并显示受控状态，应用负责提供重排数据。拒绝回写时顺序和指示不变。

多选为受控 selectedRowKeys；回调收到完整的下一组 key。可配置每行 disabled predicate，表头全选只操作当前页的可选行，保留其他页及数据源暂时不存在的 key。没有选择回写时 Checkbox 保持受控显示。loading 显示正文遮罩和 `Loading…`，阻止排序与选择，保留已有 cell；不阻止 cell 内业务组件自己的操作。

scrollY 为有限、非负的正文高度上限（默认 240），表头保持可见。横纵滚轮在正文内滚动并夹紧，边界不消费未产生位移的滚轮事件；焦点首次进入不可见 cell 时自动显露该控件。首版没有拖拽滚动条。`showHeader(false)` 隐藏表头；空数据默认 `No data`，可用第二参数 `TableEmpty{...}` 提供只挂载一次的空状态。

视觉基线固定 Ant Design 6.6.5。`ThemeConfig::table.tokens` 提供背景、表头、正文、边框、hover/selected 颜色，以及三档 padding、字体尺寸/行高、最小列宽、选择列宽、空状态高度和边框宽度。字体族/正文字重继承 Text token；表头字重为 600。Default、Dark、Compact 和嵌套覆盖均受测；颜色更新不测量或重跑 cell。LayoutStyle 只控制外部布局。

首版全量保留挂载，适合有限桌面业务数据，内存随行数×列数增长；分页只控制可见行。没有虚拟化、服务端请求管线、过滤菜单、列 schema 动态替换、固定列、复杂 span、树或内置编辑。绘制仅使用共同 logical CPU scene v3，GPU ABI v2 不变。本 change 的证据为 Windows MSVC headless，不代表 GPU、Linux、人工视觉或 IME 验收。
