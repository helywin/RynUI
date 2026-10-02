# Spec Delta

## Purpose

让原生桌面应用可以用同一数值输入合同选择多个端点、移动整个选中区间并动态编辑端点；在受控值、禁用状态、主题及窗口变化中保持可预测的键盘焦点、指针捕获、提示和资源生命周期。

## ADDED Requirements

### Requirement: Typed multiple values

MultiSlider SHALL 接受 typed reactive double 列表，最多 64 项；finite 值沿用现有 limits/marks 归一化并排序，允许重合。未提供初始值时使用两个 minimum 端点；显式空列表合法。非法值或 count/options MUST 原子拒绝。每个端点独立 Tab、拖动、键盘与提示；受控值未回写时继续显示旧值。

#### Scenario: External topology change
- **WHEN** 受控列表从 20/50/80 变为 20/37/50/80，再移除 37
- **THEN** 原有未变化端点保留 identity 与焦点，新端点独立交互，删除后不残留场景/浮层

### Requirement: Whole track gestures

双端和多端范围 SHALL 支持 draggableTrack。按选中轨道开始整段捕获，使用起始列表和平移量限制边界；规则 step 网格保持间距，不规则 marks 逐点归一化。轨道外点击仍选择最近可用端点。draggableTrack 与 editable、marksOnly 互斥；配置冲突 MUST 明确拒绝。

#### Scenario: Bounded track drag
- **WHEN** 20/50/80、step=5 的选中轨道向右拖动超过 maximum=100
- **THEN** 报告 40/70/100，成功 release 完成一次，reverse/vertical 以同一数值方向合同工作

### Requirement: Editable count bounds

MultiSlider SHALL 支持 editable 和 minCount/maxCount（0–64 且有序），当前 count 必须合法。未命中现有端点的轨道/mark 选择插入新端点，达到 maxCount 后移动最近可用端点。Delete/Backspace 删除焦点端点，跨轴拖出超过 130 logical px 后 release 删除；达到 minCount 时拒绝删除。键盘删除只在非 repeat down 完成一次。

#### Scenario: Add and remove
- **WHEN** editable 的 20/80 在轨道 50 点击后聚焦新端点并执行 Delete
- **THEN** 依次报告 20/50/80 和 20/80，原端点保持 identity，焦点移动到相邻可用端点，每次成功编辑完成一次

#### Scenario: Cancel deletion preview
- **WHEN** 拖出删除区域后触发 cancel 或窗口失焦
- **THEN** 保留端点，不报告删除或完成，清除捕获与删除预览

### Requirement: Per handle disabling

Slider SHALL 支持逐端点禁用列表和全局禁用。未提供的列表项视为 false，超过 64 项非法。禁用端点不得接受焦点/拖动/键盘或值提示；轨道与 mark 跳过它们。存在任意已渲染禁用端点时 SHALL 停止 editable 与整段拖动，其余端点仍可交互。

#### Scenario: Disabled middle handle
- **WHEN** 20/50/80 的中间端点禁用
- **THEN** Tab 和轨道选择跳过中间端点，20 与 80 仍可移动，编辑和整段拖动停止，重新启用恢复能力

### Requirement: Native focus and hint configuration

Slider SHALL 提供安全的原生 ref focus/blur 与首次挂载 autoFocus；focus 选择第一个可用端点，销毁/失效/异线程调用安全失败。hint 提供 overflow 调整配置，未指定位置时横向 Top、纵向 Right，显式 placement 优先。既有显式位置调用保持兼容。

#### Scenario: Reference lifetime
- **WHEN** ref 在组件销毁前后分别执行 focus/blur
- **THEN** 存活时按当前可用端点工作，销毁后返回失败且不访问旧 host；ref 可以在后续挂载复用

### Requirement: Retained editing lifecycle

动态数值/编辑配置 SHALL 保留未关联组件，不重新运行 Content。count 改变只增删相应端点，更新取消过期捕获和焦点；回调可重入销毁。新增端点/轨道/提示使用共同 logical scene 和 Theme；稳定帧无持续请求或上传。完成原生实现后支持清单 SHALL 准确区分功能与平台验收。

#### Scenario: Reentrant edit callback
- **WHEN** 插入或删除的 onChange 立即销毁组件
- **THEN** 不继续访问旧记录，不残留交互、浮层或完成回调，sibling 保持可用
