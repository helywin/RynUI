# 从 List 迁移到 Listy

锁定 Ant Design 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd` 已将 List 标为 deprecated，并推荐 Listy。RynUI 保留旧目录项与独立迁移页，不新增 `List` 控件，也不将废弃项目提升为 implemented。

| 旧入口 | 原生迁移入口 |
| --- | --- |
| dataSource / rowKey | `ListyProps<T>::items` / `ListyItem<T>(key,data)`，显式稳定身份 |
| renderItem | `itemRender(ListyItemView<T>)`，slot 内绑定 `data()` 和 `index()` |
| header / footer | typed `ListyContent` |
| locale.emptyText | typed `ListyEmpty` / `emptyContent` |
| grid / itemLayout | `columns` / `direction` |
| pagination | `pagination`、`current` / `defaultCurrent`、`pageSize`、`onChange` |
| scroll | `height`、`ListyRef::scrollTo`、`onScroll` |

List.Item.Meta 可组合现有 Avatar、Text、Space 或 CardMeta；应用负责自己的操作和数据服务。不能把 React render 函数复制成每次属性更新重建整行：使用 typed Binding 让保留控件局部更新。

Listy 已通过 107 的 MSVC HEADLESS Debug/Release 定向合同和 Windows hidden D3D12 操作捕获，实际范围与未覆盖的 group/sticky/virtual 见 [Listy](listy.md)。旧 List 独立说明页显示替代路径，不注册假的 List 示例；`gallery.sample.listy.retained` 提供真实控件。
