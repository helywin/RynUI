## ADDED Requirements

### Requirement: Typed offline icon component

系统 SHALL 提供 `ryn::Icon`、typed IconProps 和图标名称，使用锁定且附许可证的官方轮廓资源，离线渲染真实图形，默认继承当前 Theme 与 slot 的字号和前景。

#### Scenario: Correct icon without system symbol fonts
- **WHEN** 应用声明眼睛、隐藏眼睛、搜索或清空图标
- **THEN** 使用内嵌官方图形显示，尺寸随上下文字号和实际 DPI 更新，不依赖系统符号字体或网络

#### Scenario: Reactive style and identity
- **WHEN** 图标 name、tone 或父级主题/交互前景改变
- **THEN** 原组件身份保持，更新相应局部资源，颜色变化不重排版或重新执行无关父组件

### Requirement: Shared input action icons

Password、Input clear 与 Search 默认操作 SHALL 使用 Icon；原有受控状态、disabled、visible、pointer、键盘和焦点行为必须保留。

#### Scenario: Password toggle
- **WHEN** 用户点击或键盘激活可用的密码图标
- **THEN** 眼睛状态与密码可见状态同步，编辑值及选择不丢失；禁用时不触发回调

#### Scenario: Clear and search
- **WHEN** 用户使用清空或默认搜索入口
- **THEN** 清空显示关闭圆形、默认搜索显示放大镜，激活语义保持，自定义搜索按钮 slot 继续使用用户内容

#### Scenario: Theme and focus visibility
- **WHEN** 图标操作处于亮色/暗色、悬浮、禁用或键盘焦点状态
- **THEN** 图形前景取当前主题，普通与悬浮可辨认，禁用有独立表现，键盘焦点可见
