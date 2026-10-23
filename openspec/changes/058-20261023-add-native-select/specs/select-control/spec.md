# Spec Delta

## Purpose

为原生桌面表单提供可直接绑定业务值的单选下拉组件，统一真实选择、搜索、清空、动态选项、键盘与焦点行为，并在主题、窗口缩放和生命周期变化时保持可读、可操作与数据一致。

## ADDED Requirements

### Requirement: 单选值与选项

Select SHALL 支持字符串和有符号64位整数值、typed 选项、受控与非受控单选及空值。选中值与展示标签 MUST 分离，禁用选项不可选，重复 value 的选项集合 MUST 在发布前拒绝。

#### Scenario: 受控回写与未知值
- **WHEN** 用户请求选择一项而父级拒绝回写，或当前值暂时不在动态 options 中
- **THEN** 受控显示继续由外部值决定；未知值保留并以值文本展示，后续 options 到达后恢复对应标签

### Requirement: 可搜索与清空

Select SHALL 支持本地标签搜索、Unicode 输入与 IME、清空和空结果文案；搜索文本 MUST 不直接改变选中值，清空触发明确的空值意图与回调。

#### Scenario: 搜索不产生选择
- **WHEN** 用户输入 CJK/Latin 搜索或处于 IME 组合阶段
- **THEN** 只更新候选列表；未确认选项不更改 value，IME 拥有组合阶段的导航与提交

### Requirement: 下拉导航和窗口层

Select SHALL 提供指针、ArrowUp/Down、Home/End、Enter、Escape、Tab 与外部点击交互，键盘跳过禁用选项、活动项自动进入可见区域；长列表可滚动，弹层与 viewport 边缘相交时翻转和裁切。单个 Select MUST 只提供一个 Tab 入口。

#### Scenario: 弹层选择和关闭
- **WHEN** 用户点击候选、按 Enter 确认、按 Escape/Tab 或点击组件外
- **THEN** 有效候选提交选择意图；关闭操作不修改 value，点击候选不会因失焦提前销毁命中目标

### Requirement: 属性与生命周期

Select SHALL 提供三尺寸、四变体、status、loading、disabled、placeholder、prefix/suffix slots、ref 和打开状态控制。更新 MUST 保持组件与输入 identity；禁用、窗口失活和销毁关闭弹层并清理捕获/会话/计时。

#### Scenario: 更新和清理
- **WHEN** 挂载后变更选项、主题、尺寸或禁用状态，随后失活或销毁
- **THEN** 仅更新相关候选/布局/材质，不重跑兄弟组件，清理后无可命中遗留项或持续空闲帧
