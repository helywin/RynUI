# Design

## Context

复用 Card 的语义字体/前景色 scope、Tabs 的 ComponentLayout 与局部 clip/滚轮、既有 Button 与 Checkbox。参考锁定 Ant Design commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；本机 6.6.5 的 table/pagination style SHA256 与 sources.lock.yaml 一致。Context7 文档辅助 API 语义，默认值以锁定文件为准。

## Goals / Non-Goals

提供实际业务可用的固定 schema；不建立通用 collection/reconciliation 框架。分页首版通过 Table 的 current/pageSize 与独立 Pagination 共享 Signal 组合，不在 Table 复制分页按钮。所有行保留挂载，分页只切换 branch；有限数据规模由应用控制，不承诺大数据虚拟化。

## Decisions

- `Table<Row>` 的轻量模板适配层将每行保存为 typed Signal，cell 收到 `TableCell<Row>`（只读 Prop<Row> 与稳定 key），内部 host 只管理 key/列/已挂载行。不使用 any 字符串字段或 VDOM。比较器在 typed 层执行，不把 Row 类型泄漏进公共非模板运行时。
- 每次源变化先验证 key，再更新保留的行 Signal、增删对应 scope，排序与分页仅改变布局顺序/branch。cell 与 header slot 只挂载一次，依赖值通过 Prop/Binding 更新。删除即销毁；同 key 删除后重新插入是新生命周期。
- 排序单个受控 `TableSort`（columnKey/order），提供列 comparator 时本地 stable_sort，无 comparator 时仅输出状态与请求，应用自行重排数据；不实现网络管线。
- 选择只受控 `selectedRowKeys`，Checkbox 支持 row disabled predicate；全选保留当前页以外 key。loading 保留数据单元格但阻止排序/选择并显示独立文案。
- Table 外层 ComponentLayout 分配固定 header 和有限 body；按列 width，未设宽的列均分剩余宽度且有最小宽度。每格 box 裁剪内容，正文 clip 与节点 translation 复用现有 scene。首版滚轮支持双轴，不增加全局 Scroll 框架。
- Pagination 保留有限数量的按钮，reactive 文案/目标页/禁用，不按 total 创建全部页。Button 复用既有 focus/键盘/输入边界行为。Table 与 Pagination 分别有 typed token，颜色与度量独立订阅。

## Risks / Trade-offs

- 全量行挂载的 O(rows × columns) 内存与布局成本 → 明确首版无虚拟化，分页用于有限业务数据。
- callback 可能删除组件 → 调用前复制回调与参数，回调后不访问旧 state；清理及销毁回调加入测试。
- 动态 source 的非法 key → 结构修改前验证，保留上一次有效显示。
- 新共享接线与父任务并行 → 只添加必要独立字段/构造/target，不编辑 Gallery、README、全局 catalog、support overlay。

构建使用当前独立 worktree 的 `out/build/windows-msvc-headless`，Ninja Multi-Config、MSVC，Debug/Release focused CTest 和完整通用回归，最大 4 jobs；个人 helper 位于忽略的 out，不提交机器路径。不声称原生 GPU、Linux 或人工 IME 验收。
