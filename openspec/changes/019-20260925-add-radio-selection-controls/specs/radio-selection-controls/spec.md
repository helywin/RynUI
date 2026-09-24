# Spec Delta

## Purpose

为 RynUI 提供可独立使用或按值互斥组合的单选控件，使应用能以类型化声明、响应式状态与主题 Token 构建稳定的选项输入，并维持一致的键盘、指针和生命周期行为。

## ADDED Requirements

### Requirement: Radio 状态与激活
系统 SHALL 支持独立 Radio 的受控 `checked` 或 `defaultChecked` 模式，二者同时提供时 MUST 拒绝挂载。未选中 Radio 的有效激活 SHALL 变为选中或请求受控方回写；已选中 Radio 再激活 SHALL 不触发取消选中或重复变更。

#### Scenario: 未受控 Radio 激活
- **WHEN** 未选中且未禁用的独立 Radio 收到有效指针释放或 Space 键释放
- **THEN** Radio 变为选中并调用一次 `onChange(true)`

#### Scenario: 已选中与受控回写
- **WHEN** 已选中 Radio 再次激活，或受控 Radio 尚未收到外部状态回写
- **THEN** 已选中 Radio 不发出重复变更，受控 Radio 只发出选中请求且不自行修改外部值

### Requirement: RadioGroup 互斥值
系统 SHALL 通过静态选项数据声明 RadioGroup，每个选项具有唯一值、标签和可选禁用状态。Group SHALL 支持受控 `value` 或 `defaultValue`、组级禁用、横向或纵向布局和 `onChange`；同一时刻最多一个选项被选中，选择当前值 SHALL 不重复回调。

#### Scenario: 组内切换与受控回写
- **WHEN** 用户激活未禁用且不同于当前值的选项
- **THEN** 未受控 Group 更新唯一选中项并调用一次新值回调；受控 Group 调用一次新值回调并等待外部回写

#### Scenario: 无效选项与禁用
- **WHEN** 选项值重复、受控值与默认值同时提供，或用户激活被禁用的选项
- **THEN** 前两种声明在获取组件资源前被拒绝，禁用选项不改变值也不回调

### Requirement: 主题与稳定资源
Radio SHALL 使用主题派生的尺寸、颜色、焦点和文字样式，保留 label 的 typed slot 与点击区域；颜色变化 MUST 不触发测量、重挂载或稳定 scene topology 重建。销毁或回调自毁 SHALL 释放自身交互、scene 和布局资源而不影响同窗其他组件。

#### Scenario: 主题颜色更新
- **WHEN** Radio 的主题颜色改变而尺寸保持不变
- **THEN** 现有 surface 与子组件身份保持稳定，系统只提交所需的材质更新

#### Scenario: 生命周期清理
- **WHEN** Radio 或 RadioGroup 在回调中或随后被销毁
- **THEN** 不再交付重复激活，相关资源被释放且同窗组件保持可用
