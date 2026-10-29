# Spec Delta

## Purpose

使 Table 在选择和视觉属性变化时复用既有排序与分页投影，避免用户勾选一行触发全量排序。优化必须保持记录键、单元格编辑内容、受控状态、分页结果和键盘焦点顺序的一致性。

## ADDED Requirements

### Requirement: Table projection follows data sorting and paging inputs

系统 SHALL 仅在排序相关输入改变时重新排序；选择或 bordered 改变不得调用 comparator，分页变化应复用未改变的排序结果。数据变化须及时使投影失效。

#### Scenario: Selection and border update on a sorted table
- **WHEN** 在已有排序的 1k、5k、10k 行 Table 中单改 selectedRowKeys 或 bordered
- **THEN** comparator 调用为零，选择反馈正确，row key、cell 草稿和焦点身份保持

#### Scenario: Paging and data replacement
- **WHEN** 翻页、替换同 key 数据、增删记录或接受新的排序 authority
- **THEN** 当前页与排序结果符合新输入，未修改的排序不重复运行，保留行后代及外部邻居的正确 Tab 顺序

#### Scenario: Paging preserves the retained interaction order
- **WHEN** 只有当前页或 pageSize 改变，完整数据和排序结果保持不变
- **THEN** 复用全量已排序后代的声明顺序，由 branch eligibility 跳过非当前页；不在每次翻页重新排列全表交互，实际 Tab 路径与当前页一致

#### Scenario: Callback reentry and rejection
- **WHEN** 排序或状态交付期间回调更新、拒绝、销毁或重绑 owner
- **THEN** 不提交过时投影，不访问旧代对象，替代组件保持有效

#### Scenario: Interrupted sorting projection
- **WHEN** 排序 comparator 或投影缓冲分配抛异常，随后由普通属性变化触发重试
- **THEN** 保留原异常，完整重建排序及必要布局，不把残缺行顺序当作有效缓存，保留既有行代际
