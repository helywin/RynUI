## ADDED Requirements

### Requirement: Divider public API

系统 SHALL 提供 `ryn::Divider`，使用 typed DividerProps、reactive `Prop<T>` 与只控制外部布局的 `LayoutStyle`。类型 SHALL 覆盖 `horizontal`/`vertical`，朝向 SHALL 覆盖 `left`/`right`/`center`，并 SHALL 支持 `orientationMargin`、`dashed`、`plain` 与可选的 typed 文字 slot。Divider SHALL NOT 提供交互状态或 `disabled` 入口。

#### Scenario: Declare each divider form
- **WHEN** 应用声明水平分割线、带文字水平分割线、垂直分割线、虚线分割线与 plain 分割线
- **THEN** 每种形式使用同一公开组件声明，不要求应用直接创建线条 primitive，也不要求传入通用视觉 Modifier

#### Scenario: Reactive divider properties
- **WHEN** type、orientation、orientationMargin、dashed、plain 或文字内容在挂载后变化
- **THEN** 组件保持原有 identity，只更新受影响的几何或材质范围，不重新执行无关父组件，也不重建无关 scene fragment

#### Scenario: No interaction state
- **WHEN** 指针经过或点击任意 Divider
- **THEN** 不产生悬浮、按下、焦点或禁用状态，不参与命中与 Tab 顺序

### Requirement: Divider geometry and orientation

Divider SHALL 按 Ant Design 6.6.5 规则计算线宽、文字间距与水平/垂直外边距：水平分割线使用上下的块级间距，垂直分割线使用相对当前行高的固定高度与左右的行内间距。

#### Scenario: Horizontal divider with and without text
- **WHEN** 水平分割线没有文字或有文字
- **THEN** 无线条贯穿可用宽度并保留上下间距；有文字时线条在文字两侧按 `orientation` 断开并保留文字两侧间距

#### Scenario: Vertical divider height
- **WHEN** 垂直分割线与相邻内容在同一行内声明
- **THEN** 使用相对当前行高的固定高度并保留行内间距与相对偏移，不撑高或压缩相邻内容，宽度只由线宽与行内间距决定

#### Scenario: Orientation margin from Theme token
- **WHEN** `orientation` 为 left 或 right 且组件未显式设置 `orientationMargin`
- **THEN** 该侧轨道宽度使用 `DividerThemeToken::metrics.orientation_margin` 比例

#### Scenario: Explicit orientation margin
- **WHEN** `orientation` 为 left 或 right 且组件显式设置 `orientationMargin`
- **THEN** 该侧轨道宽度使用该比例，文字与对应边缘的距离由该比例决定，另一侧仍按分割线规则填充

#### Scenario: Left or right without orientation margin
- **WHEN** `orientation` 为 left 或 right 且既没有组件级 `orientationMargin` 也没有有效的主题比例
- **THEN** 按上游 `no-default-orientation-margin` 规则把对应轨道宽度归零，文字改用 `sizePaddingEdgeHorizontal` 边距，另一侧占满剩余宽度

#### Scenario: Text wider than the available width
- **WHEN** 带文字分割线的可用宽度小于文字与两侧间距之和
- **THEN** 文字按既有省略或换行规则退化为不溢出容器，两侧轨道收缩到零而不产生负宽度或裁掉相邻内容

### Requirement: Divider theme contract

Divider 的颜色、线宽、间距、带文字排版与 plain 文字排版 SHALL 全部来自 Theme 与 Divider Component Token，不得通过 `LayoutStyle` 或组件属性直接改写视觉。颜色变化 SHALL 只产生材质失效，不得触发重新测量或重新布局。

#### Scenario: Theme and token overrides
- **WHEN** 当前 Theme 在亮色、暗色、紧凑或自定义 `DividerThemeConfig` 下解析
- **THEN** 线条颜色取 `ant.alias.colorSplit`，带文字取 `colorTextHeading`／`fontSizeLG`／字重 500，线宽、文字间距与行内间距取 Divider Component Token

#### Scenario: Plain divider changes text only
- **WHEN** Divider 启用 `plain`
- **THEN** 文字改用 `colorText`、常规字重与 `fontSize`，线条颜色、线宽与几何保持不变

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
