# Spec Delta

## Purpose

为桌面数值输入提供可读刻度、离散候选点与当前值提示，使标记选择、键盘操作、拖动和受控显示使用一致的数值合同，并在主题和窗口变化后保持正确布局、焦点与保留场景。

## ADDED Requirements

### Requirement: Typed marks and included track

Slider 与 RangeSlider SHALL 支持 reactive typed marks、dots、included。mark 值必须 finite、唯一且在 limits 闭区间内，配置最多 4096 marks；非法变更 MUST 保留组件原状态。标签使用真实文字排版、主题前景和尺寸，支持鼠标选择；included=false 隐藏选中轨道，只标记实际选中值对应的点。

#### Scenario: Mark selection
- **WHEN** 用户点击 value=37 的标签且 step=10
- **THEN** 选择 37，mark 点和文字依横纵向/反向位置正确绘制，回调和完成行为符合输入合同

### Requirement: Common selectable values

Slider SHALL 提供 marks-only 模式：仅 min、max 与 marks 值可选；普通模式使用 step 点与 marks 的并集。归一化选最近候选，等距取较大值；方向键选择相邻点，Page 键推进十个候选或十步并考虑 marks，Home/End 使用边界。dense step 不得强行生成所有候选点；dots 仅在可表示且不超过 4096 的绘制集合中启用，否则 MUST 明确拒绝配置。

#### Scenario: Discrete keyboard
- **WHEN** marks-only 候选为 0、20、37、100，当前值为 20，用户按增加键
- **THEN** 值到 37；拖动与外部 value 使用同一候选集合，范围端点不跨越

### Requirement: Persistent value hints

Slider SHALL 提供 typed Auto/Always/Hidden 提示配置、formatter 和位置，复用窗口 Tooltip。Auto 在 thumb hover、键盘 focus 或拖动时显示，禁用 Slider 隐藏；formatter 为空字符串时隐藏。值、主题、窗口大小与锚点移动更新现有提示，不重挂载 Slider/trigger。Escape 关闭当前提示，离开激活状态后允许再次显示。

#### Scenario: Controlled value hint
- **WHEN** 受控拖动请求新值但调用者未回写 value
- **THEN** 提示和 thumb 继续显示受控 value，回写后同帧移动；回调不重挂载或抢走焦点

### Requirement: Retained themes and lifecycle

标记与提示 SHALL 使用 Theme、logical scene 与共同 renderer；颜色变更不测量无关组件。mark topology 更新和销毁清理文字/交互/浮层；稳定帧不持续请求或上传。支持状态 MUST 仍列出尚未完成的原生动态范围/整段拖动能力。

#### Scenario: Replacing marks
- **WHEN** marks 从三项变为一项后组件销毁
- **THEN** 不残留旧 label、dot、交互或 Tooltip deadline，其他 sibling 保持 identity
