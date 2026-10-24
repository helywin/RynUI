# Breadcrumb

固定 `std::vector<BreadcrumbItem>`，key 非空且唯一。项内 `title`、`disabled`、`visible` 使用 `Prop<T>`，富标题、图标和 separator 使用独立 typed slots。最后一个可见项是当前项，祖先激活回调返回原始 key/index。隐藏项不改变公开索引。拓扑改变需要重新挂载。

080 第一阶段仅提供 API/model 合同；host 完成后补充可运行示例和验收。
