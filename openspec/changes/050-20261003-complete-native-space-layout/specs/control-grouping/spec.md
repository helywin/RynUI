# Spec Delta

## Purpose

定义原生紧凑控件组的尺寸继承、方向、嵌套、相邻边界与附加标签，以及真实输入和资源生命周期合同。复用已有组件的交互、主题与逻辑场景，使组合控件在动态更新后具有正确外角、共享边和命中位置。

## ADDED Requirements

### Requirement: Compact 原生布局和上下文
SpaceCompact SHALL 提供 H/V、LTR/RTL、Small/Middle/Large、block 与 typed content；默认 horizontal、Middle。未显式 size 的受支持 child MUST 继承最近 Compact，显式 child size 优先。nested group SHALL 合并首尾边界且保留各自方向/尺寸上下文。

#### Scenario: 响应尺寸与方向
- **WHEN** 同一组 Button、Input/Password/Search、RadioButton 改变 group size/direction/block
- **THEN** 子控件保留 content、editor、interaction 与 scene 身份，继承尺寸及相邻布局更新，显式 size 不被覆盖

### Requirement: Compact 连接角和共享边
Compact SHALL 将相邻控件内角设为方角，仅保留整体首尾外角；边框 SHALL 按主题 line width 重叠形成单一 seam。hover、active/focus 和 disabled 状态 MUST 决定边的优先呈现，控件自身视觉变体、状态影子和有限反馈继续适用。

#### Scenario: 相邻状态边
- **WHEN** 中间 Input 获得 focus 或 Button hover/active，且相邻项 disabled
- **THEN** 活跃项边正确呈现，内部圆角不出现，指针与焦点命中跟随 actual bounds，有限动画完成后空闲

### Requirement: Addon 原生附加内容
SpaceAddon SHALL 提供 typed content、Outlined/Filled/Borderless/Underlined、disabled 与 Warning/Error status，尺寸来自 Compact；视觉只由 Theme/Component Token 决定，外部空间由 LayoutStyle 决定。

#### Scenario: 附加标签和主题
- **WHEN** Addon 与 Input 在 Compact 中连接并改变主题、size、variant 或 status
- **THEN** 内容与连接边保留，尺寸/颜色/边框随主题和状态更新，不执行 Web 样式入口

### Requirement: Compact 生命周期与分平台证据
Compact SHALL 支持空组/单项、窄约束、嵌套和动态 child 销毁，释放 callbacks/订阅/scene/捕获及焦点，稳定状态不得请求帧。平台通用合同与真实 Windows/Linux 字体、窗口、GPU/input/DPI 证据 MUST 分开记录。

#### Scenario: 销毁和空闲
- **WHEN** 捕获或聚焦的 child 被销毁，再销毁整体 Compact
- **THEN** 相邻边界在后续同步更新，旧 handle 不可用，资源归零；缺少 Linux 实机证据时其 checkbox 保持待完成
