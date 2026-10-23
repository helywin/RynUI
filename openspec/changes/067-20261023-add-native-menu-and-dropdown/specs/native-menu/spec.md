# Spec Delta

## Purpose

为桌面应用提供可保留节点与细粒度响应更新的原生导航菜单，明确键盘、焦点、禁用、分组与受控选择展开合同，使导航无需依赖 Web 容器或重建无关组件。

## ADDED Requirements

### Requirement: Stable native navigation

Menu SHALL 提供稳定唯一 key、响应式 label/disabled/danger、分组/分隔线与 Inline 子菜单，普通状态更新不得重执行 slot 或改变行 identity；重复/空 key、受控与默认值冲突 SHALL 在挂载时拒绝。

#### Scenario: Reactive retained rows
- **WHEN** label、禁用或选择变化
- **THEN** 相同行节点更新，禁用项及组/分隔线不能执行命令

### Requirement: Controlled requests and keyboard

Menu SHALL 支持 selectedKeys/openKeys 的独立受控请求与默认值，Up/Down/Home/End/Enter/Space 与 Inline Left/Right 导航；禁用项跳过且活动行在有限 viewport 内可滚动显示。

#### Scenario: Refused request
- **WHEN** 调用方拒绝选择或展开请求
- **THEN** 回调收到意图且实际选择/展开保持外部值

#### Scenario: Inline keyboard
- **WHEN** 键盘展开并选择子项
- **THEN** 方向键进入/返回子树，Enter 只执行合法活动项并保留稳定 key

### Requirement: Theme and cleanup

Menu SHALL 通过 Theme/Component Token 控制视觉，LayoutStyle 仅控制外部布局；卸载 SHALL 释放 interaction、surface 与订阅，不改变 CPU scene v3/GPU ABI v2。

#### Scenario: Paint update and disposal
- **WHEN** 仅颜色 token 更新后卸载菜单
- **THEN** 不重测量无关内容，卸载后无菜单资源或持续帧 deadline
