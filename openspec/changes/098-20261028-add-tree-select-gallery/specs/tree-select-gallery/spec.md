# Spec Delta

## Purpose

让使用者从组件目录进入原生 TreeSelect 的真实层级目标选择与表单流程，观察接受值、受控拒绝、键盘焦点、有限弹层和重置后的显示。Gallery 支持声明与实际验证范围一致，保留原有样例与交互身份。

## ADDED Requirements

### Requirement: 目录直达真实层级选择

Gallery SHALL 提供可从 TreeSelect 目录按钮直接到达的交互样例，展示固定层级的父／叶节点选择、展开、有限列表滚动、清空与响应式标题／节点禁用。反馈 SHALL 使用当前接受值及真实操作结果。

#### Scenario: 导航后选择并保存目标
- **WHEN** 用户点击 TreeSelect 目录入口，展开层级并选择目标，编辑备注后保存
- **THEN** 目录定位到真实样例；显示值、目标标题与保存反馈一致，禁用节点不能被选择

### Requirement: 受控请求与恢复操作

样例 SHALL 展示受控选择／关闭的拒绝与后续接受；一次外部点击只产生一次关闭请求。禁用、只读、隐藏及其恢复操作 SHALL 可通过样例外部控件访问，恢复不得擅自复活已抑制弹层。

#### Scenario: 拒绝关闭后再次操作
- **WHEN** 用户聚焦已打开的树并点击外部，父级拒绝本次关闭，随后允许下一次请求
- **THEN** 首次弹层保持打开且仅记录一次请求；下一次可实际关闭；禁用／隐藏后的恢复按钮仍可用

### Requirement: Form String 提交与同值重置

Gallery SHALL 提供真实 Form String 绑定、显式控件 ref、必填校验、成功提交和 reset。重置 SHALL 恢复接受值及显示，不制造选择事件，不替换控件、editor 或树行身份。

#### Scenario: 聚焦状态下重置
- **WHEN** 用户校验空值后选择合法节点提交，并在 selector 保持焦点时执行同值 reset
- **THEN** 必填反馈与实际字段一致，成功反馈包含提交值，重置保持合法焦点和 retained 身份；只读 selector 不建立编辑 composition session

### Requirement: 有限视口可见交互与身份保持

新流程 SHALL 在默认／暗色／紧凑主题和宽／窄有限视口中可操作；实际文字、填充、命中及适用焦点装饰遵守对应裁剪。普通属性更新 SHALL 保留样例、editor、树行及交互完整代际，键盘进入已打开弹层必须能从停稳状态请求新帧。

#### Scenario: 停稳后键盘操作
- **WHEN** 用户在已打开弹层的 selector 上以键盘进入树，再导航到列表边界
- **THEN** 无需其他操作触发同步即可请求新帧；实际焦点落在可用行，内容与命中一致，适用焦点四边可见

### Requirement: 兼容基线与支持范围

新增样例 SHALL 追加在现有样例之后，保持旧 stable IDs、旧样例函数体／调用序和非目标支持条目。TreeSelect SHALL 如实标为 partial；不得把规划、Recording 或已有平台证据描述为本轮新平台验收。

#### Scenario: 新样例与已发布功能共存
- **WHEN** 新 Gallery 被构建并执行 focused、inventory 和目录合同验证
- **THEN** Core/API/Theme/RC/系统字体/CMake 与旧值保持，旧代表流程仍通过；未覆盖首版能力与原生平台边界明确记录
