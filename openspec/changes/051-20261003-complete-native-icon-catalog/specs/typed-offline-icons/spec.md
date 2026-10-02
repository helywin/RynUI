# Spec Delta

## Purpose

为 RynUI 原生桌面应用提供可离线使用的完整 typed 图标、双色和自定义向量，同时维持主题继承、保留式更新、可控动画与窗口资源生命周期的一致行为。

## ADDED Requirements

### Requirement: Complete locked offline catalog

系统 SHALL 提供锁定官方资源包的全部 848 个 typed 图标，包含 outlined、filled、two-tone；原有十四个名称数值 MUST 保留，所有源码/许可/生成结果可校验和重现，不依赖网络或系统符号字体。

#### Scenario: Full catalog coverage
- **WHEN** 枚举并渲染所有合法图标名称
- **THEN** 每个名称均映射到非空官方图形，非法名称在改变既有可见状态前被拒绝

### Requirement: Retained layers and theme colors

图标 SHALL 默认继承 Theme/slot 字号与单色前景，双色支持 typed 主色与显式副色或由主色推导副色；layer 保留原视框、对齐与绘制顺序。LayoutStyle SHALL 只负责外部布局。

#### Scenario: Reactive two tone colors
- **WHEN** 双色主色、副色、tone 或 Theme 改变
- **THEN** 更新对应 glyph 材质，保留 Component/scene 身份且不重复 shaping、rasterization 或内容执行

### Requirement: Typed native custom vectors

系统 SHALL 接受不可变 typed 向量定义，包含有限非空 viewBox、Move/Line/Quadratic/Cubic/Close 轮廓与 primary/secondary 颜色角色；不暴露平台 renderer 或 OS SDK。定义 MUST 在提交前校验有限坐标、轮廓合法性与资源上限。

#### Scenario: Retained vector replacement
- **WHEN** 内置与自定义图形 reactive 切换或 DPI 改变
- **THEN** 使用共同离线资源路径呈现，保留布局组件身份，并正确释放已销毁窗口的资源

#### Scenario: Invalid custom vector
- **WHEN** 非有限坐标、无效轮廓、空/负 viewBox 或超限定义被提交
- **THEN** 抛出明确错误并保留此前有效定义，失败挂载不遗留节点或 scene

### Requirement: Rotation and lifecycle aware spin

图标 SHALL 支持 reactive rotate 与 spin，以图标视框中心旋转，角度更新只修改 retained geometry；spin 遵守 Theme motion 与 reduced-motion，隐藏/关闭分支和销毁后停止请求帧。

#### Scenario: Rotation without new glyph coverage
- **WHEN** 已渲染图标角度改变
- **THEN** 轮廓绕视框中心旋转、ancestor clip 正确，shape/raster/atlas 资源数量不因角度更新增加

#### Scenario: Inactive spin
- **WHEN** spin 关闭、motion 禁用、reduced-motion 启用或所在浮层关闭
- **THEN** 保留有效静态图形，不持续唤醒空帧，销毁后没有悬挂 animation callback

### Requirement: Existing shared action icons

Password、Input clear、Search 与 Typography 操作图标 SHALL 继续使用相同离线目录，保留编辑值/选择/IME、disabled、指针、键盘和焦点行为。

#### Scenario: Existing action compatibility
- **WHEN** 旧组件使用原有十四个名称并切换主题或输入状态
- **THEN** 正常显示与激活，编辑会话、清空与密码可见语义不回退
