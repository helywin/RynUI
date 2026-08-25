## Purpose

定义 RynUI 从响应状态到可见 GPU 结果的最小闭环，确保持久化 UI、布局、失效传播和局部数据更新能够以真实窗口与可观测指标验证。

## ADDED Requirements

### Requirement: 持久化 UI 挂载
声明式组件 SHALL 在首次挂载时建立持久化 UI 结构和属性依赖；普通属性状态变化 MUST 不重新执行无关组件函数。

#### Scenario: 更新绑定的颜色
- **WHEN** 已挂载元素的背景颜色状态发生变化且 UI 结构不变
- **THEN** 可见颜色 MUST 在下一次提交中更新，拥有该元素的组件函数 MUST 不重新执行

#### Scenario: 更新无关元素状态
- **WHEN** 另一个元素的状态发生变化
- **THEN** 当前元素的属性、布局结果和组件执行计数 MUST 保持不变

### Requirement: Constraints 布局结果
最小布局系统 SHALL 支持有界 Constraints 下的内部 `BoxLayout` 与 horizontal/vertical `FlexLayout` 测量和放置，并产生确定的节点边界；本 change 引入的边界 MUST 不妨碍后续提供 Ant Design `Flex`、`Space`、`Grid`、`Layout`、typed Props、typed slots 和 reactive `Prop<T>`，且不得要求稳定组件通过通用视觉 `Modifier` 才能布局。

#### Scenario: Horizontal Flex 分配子元素位置
- **WHEN** horizontal `FlexLayout` 在固定可用尺寸内包含两个具有确定尺寸的子元素
- **THEN** 两个子元素 MUST 按主轴顺序放置，且最终边界不得超出传入 Constraints

#### Scenario: 非法 Constraints
- **WHEN** 调用方提供最小值大于最大值的 Constraints
- **THEN** 系统 MUST 以可诊断的失败拒绝该测量，而不得产生未定义布局结果

### Requirement: 最小失效传播
属性更新 SHALL 只触发该属性声明的最小失效阶段，纯 Material 或 Transform 更新 MUST 不触发 Measure 或 Layout。

#### Scenario: 纯 Material 更新
- **WHEN** 已布局元素只改变颜色且几何、文本、裁剪和结构不变
- **THEN** Material 更新计数 MUST 增加，Measure 和 Layout 计数 MUST 保持不变

#### Scenario: 尺寸更新
- **WHEN** 已布局元素的宽度发生变化
- **THEN** 受影响布局根的 Measure、Layout 和 Geometry MUST 在下一帧更新

### Requirement: GPU 局部数据更新
在拓扑、裁剪和绘制顺序不变时，Material 或 Transform 变化 SHALL 通过更新现有 GPU Primitive 数据反映到下一帧，而不得重建无关 Scene 内容。

#### Scenario: 更新一个 Quad 的透明度
- **WHEN** 一个可见 Quad 只改变透明度
- **THEN** 下一帧 MUST 显示新透明度，且 Primitive 更新范围 MUST 限于该 Quad 对应的数据

### Requirement: 真实窗口闭环验证
项目 SHALL 提供一个真实窗口示例，能够通过交互或定时状态变化证明响应状态、布局、Primitive 和 GPU 输出链路有效。

#### Scenario: 交互改变可见状态
- **WHEN** 用户在示例窗口触发已绑定状态变化
- **THEN** 下一次需要的帧 MUST 显示对应变化，并输出本次 Signal、Node、Layout、Primitive 和帧提交计数

#### Scenario: 示例无变化后闲置
- **WHEN** 示例完成可见更新且没有后续输入、动画或状态变化
- **THEN** 示例 MUST 回到按需帧提交状态
