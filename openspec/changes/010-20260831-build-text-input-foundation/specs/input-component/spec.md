## Purpose

提供符合 RynUI typed component 模型和 Ant Design 6.5.0 视觉交互基线的公开单行 `Input`，让桌面表单能够安全处理 Latin、CJK 与 IME 文本。

## ADDED Requirements

### Requirement: Input 必须提供 typed Props、typed slots 与响应式 value
公开 API SHALL 提供 `ryn::Input` 和 typed `InputProps`，使用现有 `ControlSize`、typed status、`Prop<String>` value/placeholder、`Prop<bool>` disabled/readOnly、可选 maxLength、typed prefix/suffix slots、change/submit callbacks 与 `LayoutStyle`。视觉样式 MUST NOT 通过通用 `Modifier`、CSS string 或公开 renderer type 配置。

#### Scenario: 受控 Input
- **WHEN** `value` 绑定到 `Signal<String>` 且 change callback 把 committed value 写回 Signal
- **THEN** Input SHALL 显示新 value、保留可映射 caret，并且普通 value 更新不得重新执行 slot content

#### Scenario: 非受控初始值
- **WHEN** caller 只提供 `defaultValue` 而不提供 authoritative `value`
- **THEN** Input SHALL 在自身 generation 生命周期内保存 committed value，并通过相同 change callback 报告每次 transaction

### Requirement: value 与 defaultValue 必须有明确冲突规则
同一 Input 同时提供 authoritative `value` 与 `defaultValue` SHALL 被拒绝为无效 props；从 controlled 切换到 uncontrolled 或反向切换 MUST NOT 在原 generation 内静默发生。maxLength SHALL 以 Unicode scalar count 约束 committed value，composition 临时显示可超过限制但 commit MUST 原子截断到合法 cluster 边界或被拒绝，并报告确定结果。

#### Scenario: 冲突 props
- **WHEN** caller 同时设置 `value` 与 `defaultValue`
- **THEN** mount MUST 失败并给出稳定 diagnostic，不得创建可交互或 scene identity

#### Scenario: IME commit 达到 maxLength
- **WHEN** composition commit 会让 committed value 超过 maxLength
- **THEN** Input SHALL 只接受可完整容纳的 cluster，绝不拆分 UTF-8 或可呈现 cluster，并在一次 change 中报告最终 value

### Requirement: Input 布局必须覆盖 Ant Design 尺寸与 slots
small/middle/large SHALL 使用对应 Control Token 高度、font、padding、border radius 与 prefix/suffix gap；prefix、editable text、placeholder、suffix MUST 参与同一 Constraints measure，editable viewport 在空间不足时可收缩但 slots 不得重叠。`LayoutStyle` 只控制 Input root 的外部布局。

#### Scenario: 窄宽度 slots
- **WHEN** Input 同时具有 prefix/suffix 且可用宽度小于完整文本 intrinsic width
- **THEN** root 高度和 slots SHALL 保持稳定，editable viewport SHALL clip/scroll 文本并让 caret 可见，不得把 suffix 推出 HitTest bounds

#### Scenario: DPI 尺寸
- **WHEN** 同一 middle Input 在 1.0、1.25、1.5 和 2.0 logical render scale 布局
- **THEN** logical control height、padding、baseline 与 HitTest SHALL 保持 token identity，physical geometry SHALL 按 scale 对齐且不得裁切 glyph、caret 或 focus effect

### Requirement: Input 状态视觉必须遵循 Ant Design 6.5.0 Token
default、hover、active/focus、disabled、read-only、warning 与 error SHALL 只由 Theme/Input Component Token 解析。hover 只更新既有 border/background；keyboard focus-visible effect 与 active shadow SHALL 使用锁定 token geometry，pointer focus 不得增加额外蓝色 ring；warning/error active shadow MUST 使用对应 status color。

#### Scenario: Default hover 与 keyboard focus
- **WHEN** pointer hover 一个未 focused 的 default Input，随后通过 keyboard focus 进入编辑
- **THEN** hover SHALL 只改变既有 border，keyboard focus SHALL 显示独立 focus-visible effect，二者不得叠成额外粗蓝边

#### Scenario: Error active
- **WHEN** error Input 获得编辑 focus
- **THEN** border、active shadow 与 caret SHALL 消费 error status Token，不得回退为 primary 蓝色 active shadow

### Requirement: Pointer 与 keyboard 必须提供标准单行编辑体验
pointer click SHALL 把 caret 放到最近合法 text cluster，drag SHALL 扩展 selection，double-click SHALL 选择当前 word；keyboard SHALL 支持 Left/Right、Home/End、Shift selection、Ctrl/Cmd+A/C/X/V/Z/Y、Backspace/Delete、Enter submit、Escape cancel composition，并遵循 repeat 与 platform primary shortcut modifier。disabled Input 不可 focus 或编辑，read-only Input 可 focus/select/copy但不可修改。

#### Scenario: Pointer drag selection
- **WHEN** pointer down 后跨多个 cluster drag 并在 Input 外 release
- **THEN** Input SHALL 保持 capture 到 release、得到 clamp 后的 selection，并在 release 后恢复正常 hover/capture 状态

#### Scenario: Enter submit
- **WHEN** 没有 active composition 的 focused 单行 Input 收到 Enter key down
- **THEN** Input SHALL 只调用一次 submit callback，不插入换行；key repeat 或对应 key up 不得重复 submit

### Requirement: Placeholder、caret、selection 与 composition 必须正确分层
空且无 composition 时 Input SHALL 显示 placeholder；存在 value 或 composition 时 placeholder SHALL 隐藏。selection background MUST 位于 root background 之后、glyph 之前，selected glyph foreground、caret 与 composition underline MUST 位于 selection background 之上并受 viewport clip；scroll offset SHALL 让 active caret 保持可见并在 value 缩短后 clamp。

#### Scenario: 空 Input 开始 composition
- **WHEN** 空 Input 显示 placeholder 后收到非空 composition text
- **THEN** placeholder SHALL 隐藏，composition glyph、underline 与 caret SHALL 出现在 editable viewport 内，而 authoritative value 保持为空直到 commit

#### Scenario: 长文本移到末尾
- **WHEN** 单行 value 超过 viewport 且 caret 移到 End
- **THEN** Input SHALL 调整内部 horizontal offset 使 caret 可见，同时 root layout、prefix/suffix 与外部 HitTest bounds 不变

### Requirement: Reactive 更新必须限制在正确 invalidation phase
value/placeholder/font/size/slot 结构变化 SHALL 触发所需 shaping 与 Measure/Layout；hover、focus、status、caret blink、selection、composition range 与 scroll offset变化 MUST 只更新对应 Material/Geometry/Transform/HitTest domain，不得 remount Component、重跑无关 slots 或刷新 sibling scene。没有 blink deadline、input 或 dirty state 时 frame loop SHALL 恢复 idle。

#### Scenario: Caret blink 后恢复 idle
- **WHEN** focused Input 的 caret blink 完成一次 visibility transition，随后 window 失焦或 Input disabled
- **THEN** 系统 MUST 移除后续 blink deadline，提交最终静态状态并恢复 blocking idle，不得持续 submit

#### Scenario: Theme status 更新
- **WHEN** error Input 的 Theme color Token 改变而 font/size 不变
- **THEN** Input SHALL 只更新 border、shadow、caret/selection material，不重新 shape 文本或执行 Measure/Layout

### Requirement: Input 生命周期必须保持 generation-safe retained identity
Input root、editable viewport、glyph run、placeholder、selection、caret、composition underline 与 focus effect SHALL 使用 generation-checked retained identity；destroy、conditional unmount、focus transfer 与 slot reuse 后旧 callback/event/deadline MUST 被拒绝。steady-state editing MUST 不创建或删除 scene layer。

#### Scenario: focused Input 被条件卸载
- **WHEN** focused Input 在 composition 或 pointer capture 中被条件卸载
- **THEN** 系统 MUST 释放 capture/focus、停止 text input 与 caret deadline、销毁对应 retained identity，迟到事件不得影响复用 slot 的新 Input

### Requirement: Input 验收必须区分平台通用与平台专属证据
API、editing model、headless interaction、Token、scene、allocation 和 benchmark 合同 SHALL 作为平台通用任务只验收一次；Windows 与 Linux 的 native IME、candidate position、clipboard、shortcut modifier、system font、DPI、window system、GPU/shader 和真实视觉 MUST 使用各自平台 evidence 与独立 checkbox，任何平台不得代替另一平台。

#### Scenario: Windows 自动测试不能关闭 Linux 项
- **WHEN** Windows/MSVC 的 unit、headless、D3D12/DXIL 与真实中文 IME journey 已通过但 Linux Wayland/Vulkan 尚未运行
- **THEN** 平台通用和 Windows 项可以完成，Linux 与最终跨平台收口项 MUST 保持未完成
