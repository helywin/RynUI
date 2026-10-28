# Spec Delta

## Purpose

提供可在紧凑字段中选择层级节点的原生 TreeSelect。首版通过固定拓扑和单字符串值形成完整交互流程，使受控表单、键盘操作和有限空间下的树选择使用一致的 authority、裁剪和资源生命周期合同。

## ADDED Requirements

### Requirement: 单值层级选择

系统 SHALL 提供 typed、reactive TreeSelect；固定拓扑节点使用唯一非空字符串 key，空 value 表示未选。最多 1024 个节点、64 层；标题、disabled、visible 可更新。选择显示当前标题，未知 key 显示其原值；非法拓扑或互斥的 controlled/default 配置 SHALL 被拒绝。

#### Scenario: 选择父节点或叶子并更新标题
- **WHEN** 用户展开节点并激活合法父节点或叶子，随后更新其标题
- **THEN** 单选值及显示一致，普通属性更新保留 selector、树行和 editor 身份；重复激活当前值不清空值

### Requirement: 受控 authority 与事件

系统 SHALL 支持 controlled/default value、open 和展开状态。用户意图 SHALL 通知回调；受控父级拒绝时不得私改其 authority。合法选择后请求关闭，clear 请求空值；外部 disabled、hidden、失活、输入边界或零面积 SHALL 关闭实际弹层并取消输入，恢复不得擅自复活已抑制弹层。

#### Scenario: 拒绝后重试与接受
- **WHEN** 父级先拒绝选择／关闭请求，随后接受重试
- **THEN** 首次保留接受值与开合 authority，重试仍可通知；接受后显示、选中与开合一致

### Requirement: 原生输入与弹层交互

系统 SHALL 提供指针打开、原生 clear、键盘打开和树的方向键／Home／End／Enter／Space 操作；Escape 关闭最近弹层并恢复 selector，Tab 可按相邻控件顺序离开。禁用／隐藏节点不可激活，列表可在有限高度内滚动，窗口层遵守实际 popup 归属和外部裁剪。

#### Scenario: 有限视口操作
- **WHEN** 用户通过键盘打开后定位末行，选择，再以指针重新打开并滚动
- **THEN** 操作命中与可见内容一致，适用焦点轮廓有实际覆盖；弹层随祖先移动同帧定位，外部输入不被穿透

### Requirement: Form 与资源生命周期

系统 SHALL 以字符串 value/onChange 与现有 FormField<String>、显式 control(ref) 组合。reset SHALL 恢复接受值及显示，不凭空产生选择回调。首次 getter 或组合挂载异常在调用方局部捕获时 SHALL 退役原完整 generation 并保留原异常；焦点／值／开合回调销毁或重绑时不得继续使用旧状态，最终释放所有自有资源与 block。

#### Scenario: 聚焦 reset 与回调卸载
- **WHEN** 聚焦 selector 后执行同值 Form reset，或在选择／焦点回调中销毁原 owner 并重用 ref
- **THEN** reset 保持合法焦点与 retained 身份，旧 owner 停止操作，新代可用，旧 session、资源和订阅不残留

### Requirement: 独立 Theme 与兼容性

系统 SHALL 提供 TreeSelect 自有 selector/tree Token 配置，支持继承、局部算法、override、序列化/hash/equality、identity 和 dirty。最终不可渲染字号或非法度量 SHALL 在 snapshot 发布前被拒绝；旧 token 值和 identity ordinal、Core／renderer 边界及默认 1 MiB 栈 SHALL 保持。

#### Scenario: 父级算法更新与恢复
- **WHEN** 父级更新使后代 TreeSelect 最终字号非法，随后提交合法配置
- **THEN** 失败更新不发布 snapshot 或通知；合法恢复生效，非目标组件已有值保持
