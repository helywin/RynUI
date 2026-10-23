# Table 平台通用证据

2026-10-23，当前独立 worktree；Windows / MSVC 19.51 / Ninja Multi-Config，preset `windows-msvc-headless`，所有 build/CTest 最大 4 jobs。

最终 focused contract CTest：Debug 8/8（1.95 s），Release 8/8（2.08 s）：

- `rynui.table_component`：真实 Row 数据更新、重排、删除/新增、重复 key 原子拒绝、响应 cell 文本、header slot 只执行一次、列右对齐与响应宽度实际 Node bounds。
- 同一测试：受控排序拒绝/回写、稳定排序同值行、实际行放置顺序、Checkbox 受控拒绝/显示、disabled 行、当前页全选及跨页保留、独立 Pagination 组合、loading 禁用。
- 同一测试：有限正文（外部固定高度也不得扩大 scrollY）、双轴滚动夹紧、真实节点 translation、clip hit、滚动不重 shape、纯颜色不 measure、Dark/Compact、嵌套 Input 保留/删除/会话释放、callback 销毁 Table。
- 同一测试：分页外新增 autoFocus Input 与隐藏的自定义空状态不抢占编辑焦点；cell 单次 setup 的 value()/普通 Signal 读取不污染源绑定，嵌套 Binding 仍更新，异常后可继续响应有效数据。
- `rynui.table_theme`：6.6.5 Default/Dark 颜色与三档 padding、Compact、嵌套继承、独立颜色/度量失效、snapshot identity/serialization、非法 token 原子回滚。
- `rynui.table_public_api`：独立公开 Table/Pagination/Text 头与业务 Row 编译。
- `rynui.pagination_component`、`rynui.portable.prop`、`rynui.portable.reactive_source_lifecycle`、`rynui.portable.theme_algorithm`、`rynui.portable.backend_boundaries`：分页、响应合同、五组 golden 与真实 HEADLESS 依赖守卫回归。

锁定参考：Ant Design `6.6.5` / commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；读取的 table/pagination style source SHA256 分别为 `d95eb34695d22ff366654de1249193a74e7bc410051050673f9c4f7a62d94e60`、`67ff49c4ffe76155e42569525c613e0e1d9932a58539dd8e5dc524759a5125c9`，与仓库 lock 一致。五组 golden 解析比对，原有组件及 Pagination 的 token 值保持不变，只添加 Table 及更新 identity。

使用说明见 `docs/table.md`、`docs/pagination.md`。未修改 renderer/shader 或 CPU scene v3 / GPU ABI v2；没有 Gallery、README、全局 catalog/support overlay 变更。

首版全量保留行，无虚拟化、拖拽滚动条、服务端管线、复杂 span、树或内置编辑。仅有 headless 证据，不包含原生 GPU/Linux/DPI/人工视觉/系统 IME 验收。
