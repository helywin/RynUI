# Spec Delta

## Purpose

为原生桌面业务界面提供强类型行和固定列 schema 的数据表，支持持续更新、交互排序、多选和有限滚动，并用稳定行身份保留业务单元格内容及资源生命周期。

## ADDED Requirements

### Requirement: 强类型 retained 行和列
Table SHALL 接受 `Prop<vector<Row>>`、必需非空唯一 rowKey 和固定 typed 列；列支持标题或 header slot、cell slot、宽度、对齐。相同 key 的数据更新 SHALL 保留行与单元格作用域，cell 读取 reactive Row，增删只影响对应行。无效数据须在结构变动前拒绝。

#### Scenario: 行更新与重排
- **WHEN** 数据字段更新、重排行或追加删除一行
- **THEN** 保留行单元格 slot 执行次数不增加，绑定显示新值，删除行清理其子资源，外部 sibling 不重建

### Requirement: 业务交互和有限 viewport
Table SHALL 提供受控单列 sort 与回调，可选列比较器执行稳定本地排序；Checkbox 多选使用受控 selectedRowKeys 与回调，表头全选只操作当前页可选行。Table SHALL 支持响应分页切片、有限双轴 viewport、滚轮夹紧与裁剪、表头/行/边框/空/加载状态。

#### Scenario: 排序选择与分页
- **WHEN** 应用接受 sort、选择当前页行并改变 current/pageSize
- **THEN** 行顺序和切片正确，选中 key 可跨页保留，未回写的受控操作不改变显示状态

#### Scenario: 滚动和加载
- **WHEN** 内容超出有限 viewport，滚轮滚动到边界或 loading 开启
- **THEN** 表头与行列水平同步，正文垂直裁剪，滚动不重跑 cell，加载状态有明确反馈且阻止排序和选择

### Requirement: 主题和资源边界
Table SHALL 使用 Ant Design 6.6.5 锁定源对应的 typed Theme token；LayoutStyle 仅控制外部布局，绘制沿用 CPU scene v3，颜色变化不得重跑 cell 或触发布局，组件不得引入 SDL/GPU ABI 变化。

#### Scenario: 主题更新和销毁
- **WHEN** Default/Dark/Compact 或 Table token 更新，然后销毁 Table
- **THEN** 颜色与度量分别失效，nested Input/Checkbox 和 scene 资源完整释放
