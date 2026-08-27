## Purpose

定义单窗口键盘焦点、遍历、focus-visible 与激活的确定行为，使 Button 及后续输入组件共享同一 owner-thread 焦点模型，并在禁用、销毁和窗口状态变化时安全收口。

## ADDED Requirements

### Requirement: 单窗口焦点资格与顺序
系统 SHALL 在每个 Window Runtime 内维护一个 generation-checked focused identity。首批 focus order SHALL 由 eligible 组件的稳定声明顺序决定；`Tab` 前向遍历，`Shift+Tab` 反向遍历，并在首尾循环。disabled 组件 MUST 不进入 focus order；loading Button SHALL 保持可聚焦但不得激活。

#### Scenario: Tab 跳过 disabled
- **WHEN** 三个按声明顺序排列的 Button 中第二个 disabled，且用户连续按 `Tab`
- **THEN** focus 从第一个移动到第三个并循环回第一个，不停留在第二个

#### Scenario: 反向遍历
- **WHEN** 第一个 eligible Button 已聚焦且用户按 `Shift+Tab`
- **THEN** focus 移动到最后一个 eligible Button

#### Scenario: loading 保留焦点资格
- **WHEN** 一个已聚焦 Button 的 reactive loading 从 false 变为 true
- **THEN** 该 Button 保持 focused identity 和 tab order，但后续键盘激活不产生 click

### Requirement: focus-visible 由输入 modality 决定
系统 SHALL 区分 keyboard 与 pointer focus modality。由 `Tab`/`Shift+Tab` 或键盘导航获得的 focus SHALL 显示 focus-visible；pointer down 获得的 focus SHALL 不显示 focus-visible。窗口未激活时 MUST 隐藏 focus-visible 视觉，但不得把它误报为 disabled。

#### Scenario: 键盘焦点显示 ring
- **WHEN** Button 通过 `Tab` 获得 focus
- **THEN** Button 进入 focused 与 focus-visible 状态，并只更新该 Button 的 focus visual range

#### Scenario: pointer focus 不显示 ring
- **WHEN** Button 通过 pointer down 获得 focus
- **THEN** Button 进入 focused 状态但 focus-visible 为 false，click 语义不受影响

### Requirement: Button 键盘激活语义
系统 SHALL 让 focused 且 enabled、非 loading 的 Button 响应 `Enter` 与 `Space`。非重复 `Enter` key down SHALL 产生一次 click；`Space` key down SHALL 进入 pressed，匹配的 key up SHALL 产生一次 click。key repeat、失焦、disable、loading、cancel 或 target 销毁 MUST 不产生额外 click。

#### Scenario: Enter 单次激活
- **WHEN** focused Button 收到一次 `Enter` key down、若干 repeat 和 key up
- **THEN** `onClick` 只执行一次，且不因 repeat 或 key up 重复执行

#### Scenario: Space 在抬起时激活
- **WHEN** focused Button 收到 `Space` key down 后仍保持 eligible，并收到匹配 key up
- **THEN** Button 在 key down 时显示 pressed，在 key up 时清除 pressed 并执行一次 `onClick`

#### Scenario: Space 中途失去资格
- **WHEN** `Space` 已按下后 Button 变为 disabled/loading、失焦、被销毁或窗口失焦
- **THEN** pressed 被取消，后续 key up 不执行 `onClick`

### Requirement: Focus 生命周期与回调重入安全
系统 SHALL 在 focused target 销毁或变为 disabled 时清除 stale focus；窗口失焦 SHALL 取消进行中的键盘按压并暂停 focus-visible，窗口重新获得焦点后只有仍有效的 focused identity 可以恢复视觉。focus/click handler 造成结构或属性变化时 MUST 在下一步派发前重新校验 identity。

#### Scenario: 销毁 focused target
- **WHEN** focused Button 的 Scope 被销毁且 slot 随后复用
- **THEN** focus 不转移到复用 slot，新组件不会收到旧 Button 的 keyboard event

#### Scenario: 窗口失焦再恢复
- **WHEN** 窗口在 focused Button 存活时失焦后重新获得焦点
- **THEN** 进行中的按压已取消；仅当原 focused identity 仍有效且 eligible 时恢复其 focus 状态

