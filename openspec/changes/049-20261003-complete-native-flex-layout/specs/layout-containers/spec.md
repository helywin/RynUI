# Spec Delta

## Purpose

定义 RynUI 原生 Flex 的完整换行、方向、字体基线与弹性空间分配合同，保持 typed API、retained 身份和共同 CPU 布局边界，使原生文字及控件在窗口和主题变化时获得确定的对齐结果。

## ADDED Requirements

### Requirement: Flex typed 换行支持反向交叉轴
Flex SHALL 保留 bool wrap 源码兼容，并提供 NoWrap、Wrap、WrapReverse 的 reactive typed 入口；反向换行 MUST 保持每行主轴顺序、双轴 gap、声明与键盘顺序，仅翻转交叉轴的行堆叠和 flow 起止边。

#### Scenario: 动态反向换行
- **WHEN** 同一组内容在有限宽度或高度约束中切换 Wrap 与 WrapReverse
- **THEN** line breaks 和 child identity 保持一致，行从相反交叉轴边界堆叠，普通更新不重新执行 content

#### Scenario: 单行和空组
- **WHEN** 容器只有一行或无 child
- **THEN** 反向模式不产生额外 gap、非法尺寸或持续帧请求

### Requirement: Flex 对齐真实文字基线
Flex 和 LayoutStyle align-self SHALL 提供 Baseline；横向 line MUST 使用实际测量的第一条文字基线并考虑上下 margin、容器 padding 与控件内部对齐；无文字项从下边缘合成，纵向布局按交叉轴起始回退。基线行高度 MUST 容纳最大基线上方与下方范围。

#### Scenario: 混合字号和控件
- **WHEN** 同一行包含不同字号的中英文 Text、Typography、Button 或 Input，align 为 Baseline
- **THEN** 可用文字基线在 logical 坐标对齐，控件内部 padding/居中偏移正确，未参与项保留自身 align-self

#### Scenario: 字体或宽度变化
- **WHEN** 主题改变字号、文本换行或 child 主轴分配变化
- **THEN** 系统更新真实基线和行高，旧测量缓存不得保留错误基线，content/scene/focus identity 保持

### Requirement: Flex 区分逻辑方向与物理对齐
Flex SHALL 提供 reactive 原生 LTR/RTL；横向主轴与纵向交叉轴按 direction 布置，原有 Start/End 表示 flow 起止。物理 Left/Right justify MUST 在横向主轴固定对应物理边，纵向按 Start 回退；上游等价别名 MUST 映射已有 typed 含义，不引入 CSS parser。

#### Scenario: RTL 与反向换行组合
- **WHEN** horizontal/vertical wrapped Flex 同时切换 direction 与 WrapReverse
- **THEN** 主轴声明位置、行堆叠和 Start/End 具有确定坐标，键盘顺序保持声明顺序，命中与视觉 bounds 一致

### Requirement: Flex 默认 Stretch 和原生弹性值映射
Flex SHALL 默认 Stretch，对显式交叉轴尺寸及 min/max 保持尊重；显式 Start 保留旧默认行为。上游 flex 的 grow/shrink/basis SHALL 通过 LayoutStyle typed 外部布局字段表达，并保留有限非负验证、按权重分配与约束冻结合同。

#### Scenario: 默认拉伸迁移
- **WHEN** 旧调用希望保留顶部对齐且显式指定 Start，或新调用省略 align
- **THEN** 前者保持旧位置，后者自动尺寸 child 拉伸且固定尺寸 child 不被覆盖

#### Scenario: 弹性数值保持视觉身份
- **WHEN** grow、shrink、basis、align-self 或 order 的 Signal 改变
- **THEN** 只有外部空间分配和必要测量/放置变化，不改变组件 token、scene 身份或挂载次数

### Requirement: Flex 布局失效保持局部与空闲
Flex 普通 Props 更新 SHALL 保留 retained 生命周期；基线相关变化 MUST 更新测量合同，纯 justify/RTL MUST 只更新必要 placement/geometry/HitTest；销毁 MUST 释放订阅，稳定状态不得存在动画 deadline 或新帧请求。

#### Scenario: 对齐更新不重塑文字
- **WHEN** 文本及测量约束保持稳定而 justify/RTL 改变
- **THEN** Text shape 次数保持，未关联 sibling 不测量，指针命中随新 bounds 更新

### Requirement: 原生 Flex 分平台验证
系统 SHALL 独立记录共同逻辑测试以及实际平台的字体、窗口、GPU、输入和 DPI 验收，Gallery 目录 MUST 与已实现原生功能一致；无实际平台运行不得勾选对应验收。

#### Scenario: Windows 验收和 Linux 待办
- **WHEN** 当前机器完成 Windows Debug/Release 原生运行
- **THEN** Windows 项记录实际结果，Linux 项独立待完成，不重复共同逻辑合同
