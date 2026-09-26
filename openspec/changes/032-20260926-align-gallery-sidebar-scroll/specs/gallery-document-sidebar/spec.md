## ADDED Requirements

### Requirement: 组件文档式左侧导航
RynUI SHALL 在固定顶部站点栏下的宽窗口左栏按文档入口、组件类别与该类别组件条目的纵向层级展示锁定的 Ant Design 6.6.5 目录，类别与条目顺序 SHALL 与目录数据一致；导航项使用 Theme 与 Component Token 控制组件内部视觉。

#### Scenario: 查看组件类别
- **WHEN** Gallery 在宽窗口初始挂载
- **THEN** General、Layout、Navigation 等类别及其组件条目按纵向顺序出现，不再排成换行按钮云

### Requirement: 保留导航交互
RynUI SHALL 保留 section/category 跳转和支持状态筛选，导航滚动不触发正文重挂载；组件名称在没有单组件文档页时 SHALL 不表现为可打开独立页面的链接。

#### Scenario: 跳转至组件类别
- **WHEN** 用户激活左栏的组件类别入口
- **THEN** 正文滚至对应目录类别附近，左栏自身 offset 不被该跳转改写
