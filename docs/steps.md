# Steps

固定 `std::vector<StepsItem>`，key 非空且唯一。`current` 为受控 `Prop<std::size_t>`，`defaultCurrent` 为默认索引，两者互斥；默认 0。`current >= count` 表示全部完成，不回写 current。单项 `status` 优先于按原始索引派生的状态。只有 `onChange` 存在才启用导航，受控拒绝可重试。

标题、副标题、说明、禁用、可见和状态响应式更新；typed 富 slots 只挂载一次。拓扑改变需要重新挂载。080 第一阶段仅提供 API/model 合同；host 完成后补充可运行示例和验收。
