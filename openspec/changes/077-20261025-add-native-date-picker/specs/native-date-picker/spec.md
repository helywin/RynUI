# Spec Delta

## Purpose

为 RynUI 业务界面提供无时区的公历单日期录入与真实月历选择，使调用方可以通过保留组件、受控属性和统一主题处理日期筛选及编辑。该能力明确区分日期值、尚未提交的输入草稿和浏览面板状态，不以字符串猜测或 Web 控件替代原生交互。

## ADDED Requirements

### Requirement: Typed 日期与受控单选

DatePicker SHALL 使用可验证的公历年月日及可空单选值，支持受控 value、defaultValue 和明确的变更请求；日期范围为 0001-01-01 至 9999-12-31，不接受不存在的日期。

#### Scenario: 闰日与受控拒绝
- **WHEN** 调用方提供合法闰日并在用户选择另一天后拒绝回写
- **THEN** 控件发出一次准确的选择请求，显示值仍由调用方决定；非法年月日不能作为已提交日期

### Requirement: ISO 输入与清空

DatePicker SHALL 提供固定 ISO `YYYY-MM-DD` 输入、清空、disabled、inputReadOnly、三尺寸及已有 Input 状态/变体；无效草稿、禁用日期和未提交 IME 内容不得发布成选择值。

#### Scenario: 无效草稿与输入提交
- **WHEN** 用户先输入非法日期，再提交合法日期或执行清空
- **THEN** 非法草稿不改变选择值，合法提交或清空产生正确请求，普通主题/状态更新保留编辑身份；blur 后无效草稿恢复当前选择显示

### Requirement: 月历浏览和日期限制

DatePicker SHALL 提供可操作的月历、跨月/跨年导航、键盘方向键/PageUp/PageDown/Home/End/Enter 选择，限制 minDate/maxDate 和 disabledDate；限制全部日期时不得无限搜索或提交禁用日期。

#### Scenario: 跨月选择与范围更新
- **WHEN** 用户跨年浏览月历，或父级响应式修改日期范围/禁用规则
- **THEN** 月历反映实际公历日期，禁用日期不可提交，导航遵守有效范围，受控值不会被隐式改写

### Requirement: 保留弹层与输入边界

DatePicker SHALL 支持 open/defaultOpen/onOpenChange、外部点击/Escape/选择关闭、焦点恢复、typed footer 内容和受控拒绝；Modal/Spin 或隐藏祖先失活时不得继续显示或处理输入，旧弹层不得在无明确打开动作时复活。

#### Scenario: 隐藏及嵌套弹层
- **WHEN** 月历在 Modal 中打开，随后被另一个模态边界或 Spin/祖先隐藏阻挡，再恢复祖先
- **THEN** 月历和输入会话安全清理；footer 的真实控件身份保留，恢复后需要明确打开动作；更高子弹层优先接收输入

### Requirement: Theme 与有限几何

DatePicker SHALL 通过 Theme 控制视觉，支持默认/Dark/Compact、继承、局部 algorithm/override 与颜色/度量 dirty 分离；有限窗口/祖先裁剪约束实际月历 glyph、表面和命中，更新不得重挂载无关内容。

#### Scenario: 窄窗口与 Theme 切换
- **WHEN** 在窄窗口或短视口打开月历并切换三个主题
- **THEN** 弹层有界定位，实际可见内容及命中受有效 clip 约束；Input/footer 身份保留，颜色更新不触发无关布局
