## ADDED Requirements

### Requirement: Divider public API

系统 SHALL 提供 `ryn::Divider`，使用 typed DividerProps、reactive `Prop<T>` 与只控制外部布局的 `LayoutStyle`。类型 SHALL 覆盖 `horizontal`/`vertical`，朝向 SHALL 覆盖 `left`/`right`/`center`，并 SHALL 支持 `orientationMargin`、`dashed`、`plain`、禁用状态与可选的 typed 文字 slot。

#### Scenario: Declare each divider form
- **WHEN** 应用声明水平分割线、带文字水平分割线、垂直分割线、虚线分割线与 plain 分割线
- **THEN** 每种形式使用同一公开组件声明，不要求应用直接创建线条 primitive，也不要求传入通用视觉 Modifier

#### Scenario: Reactive divider properties
- **WHEN** type、orientation、orientationMargin、dashed、plain 或文字内容在挂载后变化
- **THEN** 组件保持原有 identity，只更新受影响的几何或材质范围，不重新执行无关父组件，也不重建无关 scene fragment

### Requirement: Divider geometry and orientation

Divider SHALL 按 Ant Design 6.6.5 规则计算线宽、文字间距与水平/垂直外边距：水平分割线使用上下的块级间距，垂直分割线使用左右的行内间距并延续可用高度。

#### Scenario: Horizontal divider with and without text
- **WHEN** 水平分割线没有文字或有文字
- **THEN** 无线条贯穿可用宽度并保留上下间距；有文字时线条在文字两侧按 `orientation` 断开并保留文字两侧间距

#### Scenario: Vertical divider height
- **WHEN** 垂直分割线与相邻内容在同一行内声明
- **THEN** 分割线延续该行可用高度，不撑高或压缩相邻内容，宽度只由线宽与行内间距决定

#### Scenario: Orientation margin
- **WHEN** `orientation` 为 left 或 right 并设置 `orientationMargin`
- **THEN** 文字与对应边缘的距离使用该值，另一侧仍按分割线规则填充；未设置时使用 Component Token 默认值

### Requirement: Divider theme contract

Divider 的颜色、线宽、间距与浅色变体 SHALL 全部来自 Theme 与 Divider Component Token，不得通过 `LayoutStyle` 或组件属性直接改写视觉。

#### Scenario: Theme and token overrides
- **WHEN** 当前 Theme 在亮色、暗色、紧凑或自定义 `DividerThemeConfig` 下解析
- **THEN** 线条颜色取 `ant.alias.colorSplit`，文字取对应正文颜色，`plain` 使用更浅的填充色，线宽、文字间距与行内间距取 Divider Component Token

#### Scenario: Color-only update stays local
- **WHEN** 主题只改变分割线颜色
- **THEN** 只更新材质数据，不触发重新测量、重新布局或重建无关组件子树

#### Scenario: References stay inside the locked baseline
- **WHEN** 实现引用 Divider 设计值
- **THEN** 引用 `design-tokens/ant-design/6.6.5` 中锁定的 Token identity，不通过运行时联网或临时查询选择同名值

### Requirement: Divider integration in the reference gallery

Gallery SHALL 把 `ant.component.divider` 从仅元数据条目升级为带真实样例的可浏览条目，并同步 support overlay 的 `supported_scope`、`missing_scope` 与证据标识。

#### Scenario: Browse the divider entry
- **WHEN** 用户在 Token Gallery 打开 Divider 条目
- **THEN** 可以看到水平、带文字、垂直、虚线与 plain 的真实 RynUI 样例，支持状态与说明反映本 change 的实际覆盖范围

#### Scenario: Overlay stays verifiable
- **WHEN** 运行 Gallery 参考目录合同测试
- **THEN** support overlay 与 reference catalog 的 identity、支持状态和证据标识一致，不使用过期或占位的说明文字
