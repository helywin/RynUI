# Spec Delta

## Purpose

提供符合 RynUI typed component 模型和锁定 Ant Design 6.5.0 参考状态的 Switch 与 Checkbox，使应用能通过响应式布尔状态表达即时开关和表单勾选，而不复制 Button 的完整实现。

## ADDED Requirements

### Requirement: Switch 与 Checkbox 提供明确的 typed 状态 API
系统 SHALL 在 `ryn` 命名空间公开 `Switch`、`Checkbox` 及各自 typed Props。两者 SHALL 支持 reactive checked 或非受控 defaultChecked、reactive disabled、`onChange(bool)`、适用的 typed size 与外部 `LayoutStyle`；Checkbox SHALL 提供 typed label content 与 reactive indeterminate，Switch SHALL 提供 loading。checked 与 defaultChecked 同时提供或在同一 generation 内静默切换控制模式 MUST 被拒绝。公开 API MUST 不暴露任意颜色、边框、内部 identity、SDL3 或 renderer 类型。

#### Scenario: 受控值与回写
- **WHEN** Switch 或 Checkbox 的 checked 绑定到 `Signal<bool>`，用户完成一次有效操作且 `onChange` 将新值写回 Signal
- **THEN** 回调只报告一次目标布尔值，组件按 authoritative checked 值更新；普通值变化不重新执行 label slot 或无关组件

#### Scenario: 非受控值与冲突
- **WHEN** 组件只提供 defaultChecked，或同时提供 checked 与 defaultChecked
- **THEN** 前者在本 generation 内保存切换后的值；后者在 mount 时明确拒绝且不留下交互或 scene identity

### Requirement: 两种控件遵守各自的交互语义
enabled、非 loading 的 Switch SHALL 以主 pointer 完整 click 或聚焦后的 Space 切换 checked；Checkbox SHALL 以主 pointer 完整 click 或聚焦后的 Space 切换 checked。Checkbox 的 indeterminate 是独立于 checked 的展示属性；有效用户切换 SHALL 报告目标 checked 值，但不得隐式更改外部 indeterminate 属性。disabled 控件 MUST 不可激活或进入 Tab 顺序；loading Switch MUST 抑制切换但保持已有效的焦点身份。键盘 repeat、拖出、cancel、失焦和销毁 MUST 不产生额外回调。

#### Scenario: Space 切换与 repeat
- **WHEN** 聚焦的 enabled Checkbox 收到 Space down、重复 down 和匹配的 up
- **THEN** 按压视觉在 down 后出现，只在有效 up 后产生一次 `onChange`，重复事件不重复切换

#### Scenario: disabled 与 loading 优先级
- **WHEN** disabled Checkbox 或 loading Switch 收到 pointer/keyboard 激活，同时存在 hover 或 pressed 状态
- **THEN** 不触发 `onChange`，按压状态安全取消，视觉使用对应 disabled/loading 优先级

### Requirement: 视觉、布局与焦点遵循锁定参考及 Theme
Switch 的轨道、滑块和 loading 指示，以及 Checkbox 的方框、勾号、indeterminate 横线与 label SHALL 消费 Theme/Component Token；default、checked、hover、pressed、focus-visible 和 disabled 状态 SHALL 与锁定 Ant Design 6.5.0 参考具有可区分的层级。`LayoutStyle` 只控制外部放置；focus-visible 效果与 pointer focus MUST 分离，不能把 Button 的 1px gap + 3px ring 原样套给不同控件。Small/Middle/Large 或控件所允许的尺寸 SHALL 保持 logical geometry、hit bounds 和文本可读性一致。

#### Scenario: pointer 与键盘焦点
- **WHEN** pointer down 后焦点转移到 Switch，随后用户通过 Tab 聚焦 Checkbox
- **THEN** Switch 的 pointer focus 不出现键盘专属外圈，Checkbox 呈现其 token 定义的 focus-visible 效果，二者的 hit bounds 与 scene clip 正确

#### Scenario: 主题与缩放
- **WHEN** Default、Dark、Compact 主题及 1.0、1.25、1.5、2.0 render scale 下展示 checked、indeterminate、disabled、loading 状态
- **THEN** 背景、前景、边框、指示符与 CJK/Latin label 均可辨认，视觉不被裁切或与相邻控件重叠

### Requirement: 控件更新与验收维持稳定身份
Switch/Checkbox 的 root、label、track/box、indicator 与 focus effect SHALL 在同一 generation 内保持 retained identity。纯 checked、hover、pressed、focus、loading phase 或 Theme color 更新 MUST 不 remount、不重跑无关 slot、不重新 shape 未变化文本，也不得全量刷新兄弟 scene。平台通用 API/交互/Theme/scene/benchmark 在一个正式 preset 验证一次；实际 Windows 与 Linux 的窗口、输入、字体、GPU/shader 和 DPI 视觉 SHALL 分别留证。

#### Scenario: 稳态反复切换
- **WHEN** 用户反复切换一个 Switch，周围存在 Button、Input 与 Checkbox，且 label 和布局尺寸不变
- **THEN** 仅 Switch 必要的 retained range 更新，兄弟 identity 与 content closure 不变；停止操作和动画后无持续 frame submit

#### Scenario: Windows 结果不替代 Linux
- **WHEN** Windows/MSVC 实窗验证通过但原生 Linux Wayland 窗口尚未验证
- **THEN** 平台通用与 Windows 项可独立完成，Linux 及最终跨平台收口项保持未完成
