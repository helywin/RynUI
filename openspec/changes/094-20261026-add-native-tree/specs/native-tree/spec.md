# Spec Delta

## Purpose

提供固定层级和稳定 key 的原生 Tree，使应用能够通过真实指针、键盘和有限视口浏览层级数据并选择或勾选节点，同时保留富内容身份、响应式展示与完整主题合同。

## ADDED Requirements

### Requirement: 固定拓扑与响应式展示
Tree SHALL 提供 typed Props 和富标题、图标 slots，挂载时校验非空唯一 key、最多 1024 节点和 64 层深度，超限明确拒绝。标题、visible、disabled SHALL 响应更新而不重挂富内容。隐藏及折叠节点 SHALL 退出布局且无额外间隙。

#### Scenario: 展示更新保留富输入
- **WHEN** 更新标题、隐藏并恢复节点或折叠再展开祖先
- **THEN** 富标题中的 Input、Button、Select 保留身份及输入草稿；隐藏区域不占布局，不接受输入

### Requirement: 展开和单选
Tree SHALL 支持互斥的受控/默认 expandedKeys 与 selectedKeys。交互 SHALL 发出候选值；受控状态以 prop 为准。单选最多保留一个有效 key；未知与重复 key SHALL 被规范化。禁用节点不可直接选择或展开。

#### Scenario: 受控展开
- **WHEN** 点击展开按钮而外部 expandedKeys 未更新
- **THEN** onExpand 收到候选集合但实际展开状态保持不变

### Requirement: 级联勾选
可选 checkbox SHALL 支持受控/默认 checkedKeys、父子传播和 indeterminate。disabled 节点 SHALL 阻断传播，其状态不参与父节点的全选判断；禁用子树内独立有效节点仍可操作。显隐不改变拓扑勾选语义。

#### Scenario: 禁用隔离
- **WHEN** 勾选包含禁用分支的父节点
- **THEN** 仅勾选可传播分支，禁用分支保持原状态；部分可用子节点选中时父节点半选

### Requirement: 原生导航和视口
Tree 行 SHALL 仅有一个 Tab 入口，支持 Up/Down、Left/Right、Home/End、Enter 单选、Space 勾选或单选及指针分区。富内容自身输入 SHALL 保留原行为。焦点移动 SHALL 滚动显露；有限视口下内容、填充、焦点装饰与命中 SHALL 使用各自实际裁剪。

#### Scenario: 折叠后的导航
- **WHEN** 折叠含当前焦点的后代并继续 Tab 或方向键操作
- **THEN** 焦点恢复到有效行，隐藏后代不进入顺序；有效富 child 不因行 wrapper 禁用而失效

#### Scenario: 首末行焦点与硬裁剪
- **WHEN** 有限视口足以容纳行和 Theme 焦点装饰，使用 Home 或 End 显露首末行
- **THEN** 该行焦点轮廓四边可见，必要留白计入自然尺寸与滚动范围；内容和命中仍受树视口约束，外部文档裁剪仍生效

### Requirement: 生命周期与输入边界
隐藏、折叠、禁用或 window inactive SHALL 关闭后代 popup、capture、focus、session 和 preedit；恢复不得重开旧 popup。任何用户回调销毁 owner 或私有结构后 SHALL 停止旧代际操作，安全清理本代资源和 owned input blocks，不影响 foreign owner 或新代际。

#### Scenario: 首次富 slot 销毁
- **WHEN** 首次挂载的 slot 或 Prop 读取回调销毁 Tree 或其私有区域
- **THEN** 不访问失效状态，不泄漏资源，不伤及回调创建的新组件

### Requirement: 完整 Theme 合同
Tree SHALL 仅经 Theme/Component Token 控制视觉；支持 derive、override、algorithm、继承、序列化、hash、equality、dirty 和追加 TokenIdentity。最终字体 SHALL 按真实 round 的 FontRuntime 上下限校验，失败更新 SHALL 原子回滚并允许恢复。

#### Scenario: Theme 无效更新
- **WHEN** 父 Theme 更新导致后代 Tree 字体无法解析
- **THEN** 整个继承链保留旧快照，后续有效更新成功；旧组件 typed 值与序列化片段保持原值
