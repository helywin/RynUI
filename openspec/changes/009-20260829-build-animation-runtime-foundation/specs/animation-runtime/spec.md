## Purpose

为 RynUI 提供统一、确定且按需运行的基础动画合同，使组件状态能够连续过渡、可靠取消并遵守 motion policy，同时保持 retained identity、最小失效和窗口空闲能力。

## ADDED Requirements

### Requirement: 确定性单调时间
动画系统 SHALL 使用单调时间计算状态，并 SHALL 允许测试以受控时间源重放完全相同的动画结果；同一时间点的重复采样不得推进状态，早于最近采样点的时间不得导致进度或输出回退。

#### Scenario: 受控时间重放
- **WHEN** 两次运行以相同起点、动画参数和受控时间序列采样
- **THEN** 两次运行 MUST 产生逐采样点相同的值、生命周期事件和 diagnostics

#### Scenario: 时间不回退
- **WHEN** 动画已在时间 `t1` 采样后收到早于 `t1` 的采样时间
- **THEN** 动画进度和输出 SHALL 保持在 `t1` 的状态，且系统 SHALL 记录非单调时间 diagnostics 而不得倒放

### Requirement: generation-checked 生命周期
每个动画 identity SHALL 同时包含 slot 与 generation；destroy、Scope cleanup、slot reuse 或 callback 重入后，对旧 identity 的 update、cancel、finish 和 retarget MUST 被安全拒绝，且不得访问已销毁 owner 或新 generation 的状态。

#### Scenario: 销毁后复用 slot
- **WHEN** 一个动画被销毁且其 slot 被另一动画复用，随后旧 identity 发起操作
- **THEN** 操作 MUST 被识别为 stale 并拒绝，且新动画的值、生命周期和 owner 不得变化

#### Scenario: callback 中销毁 owner
- **WHEN** apply 或 completion callback 在当前 tick 中销毁动画 owner 及其其他动画
- **THEN** 当前 tick SHALL 在每次后续访问前重新验证 identity，且不得调用已销毁 owner 的 callback

### Requirement: typed tween 与 easing
动画系统 SHALL 为 `float`、`Color`、logical geometry 和 transform value 提供 typed tween；easing SHALL 使用 validated linear、cubic-bezier 或命名 preset，不得以 CSS 字符串或未验证的任意字符串进入 Runtime。

#### Scenario: typed value 插值
- **WHEN** 支持的 typed value 在 normalized progress `0`、中间值和 `1` 被采样
- **THEN** 输出 SHALL 分别精确等于起点、按该类型规则插值得到的中间值和终点，且不得发生类型转换或字符串解析

#### Scenario: cubic-bezier 参数验证
- **WHEN** cubic-bezier 的两个 x control point 不在 `[0, 1]`、参数包含非有限数或曲线无法形成有效时间映射
- **THEN** 动画创建 MUST 失败并返回可诊断错误，不得静默替换 easing

#### Scenario: overshoot easing
- **WHEN** 合法 easing 的 y control point 产生小于 `0` 或大于 `1` 的输出
- **THEN** 系统 SHALL clamp normalized time domain，但 MUST 保留 easing output 的 overshoot，并由目标类型或消费方决定最终值域

### Requirement: 明确的时序与控制语义
动画 SHALL 定义 delay、duration、play、finish、cancel、retarget 和 interruption 的一致语义；delay 前保持起点，duration 终点精确提交目标值，零 duration 直接完成且不得建立持续 frame task。

#### Scenario: delay 与终点
- **WHEN** 动画包含非零 delay 和 duration
- **THEN** delay 结束前输出 SHALL 为起点，active interval 内按 easing 采样，到达结束时间时 SHALL 精确提交终点且只完成一次

#### Scenario: cancel 保留当前值
- **WHEN** active 动画在时间 `t` 被 cancel
- **THEN** 系统 MUST 先以 `t` 采样并保留当前值，移除后续 deadline，且不得提交原目标终点

#### Scenario: finish 提交目标
- **WHEN** pending 或 active 动画被 finish
- **THEN** 系统 MUST 精确提交目标值、只发出一次 completion，并立即移除后续 deadline

#### Scenario: 连续 retarget
- **WHEN** active 动画在时间 `t` 被 retarget 到新目标
- **THEN** 新动画 SHALL 从同一时间点旧动画的当前采样值开始，不得出现数值跳变，并 SHALL 使用新的 timing specification

### Requirement: 按需 frame deadline
活动动画 SHALL 依据 nominal display cadence 提供下一次 frame deadline，并与 input、resize 或其他 frame wake 合并；系统 MUST 使用绝对单调时间采样并跳过已错过的 cadence，不得排队补交历史帧或形成 busy loop。

#### Scenario: 多来源 wake 合并
- **WHEN** animation deadline 与 input frame request 在同一提交窗口内到达
- **THEN** frame loop SHALL 合并为一次 frame pipeline 执行，所有动画使用同一 frame timestamp

#### Scenario: 不同 display cadence
- **WHEN** 相同动画分别以 60 Hz、120 Hz 和 144 Hz nominal cadence 运行
- **THEN** 每次运行 SHALL 在相同绝对结束时间精确到达终点，采样数量可不同但不得改变最终值或完成次数

#### Scenario: 最后一个动画完成
- **WHEN** 最后一个 active 动画完成、cancel 或被销毁且没有其他 frame request
- **THEN** frame loop MUST 移除 animation deadline 并恢复 blocking idle，不得继续 submit 或轮询

#### Scenario: GPU submit 暂缓
- **WHEN** frame pipeline 因 GPU resource unavailable 暂缓 submit
- **THEN** 动画 SHALL 保留所需 dirty state 并在下一次允许的 frame 重试，但相同 frame timestamp 不得重复推进或重复 completion

### Requirement: Ant Design motion policy
Theme SHALL 将 `motion`、`motionUnit`、`motionBase`、fast/mid/slow duration 与 easing Token 映射为 typed motion specification；Ant Design 6.5.0 默认 fast/mid/slow duration SHALL 分别为 `0.1s`、`0.2s`、`0.3s`。

#### Scenario: 默认 Token 映射
- **WHEN** 使用默认 Theme 创建 fast、mid 和 slow motion specification
- **THEN** specification SHALL 保留对应 duration 与 typed easing identity，不得在组件中复制 magic number 或 CSS value

#### Scenario: motion disabled
- **WHEN** Theme `motion=false`
- **THEN** 非必要动画 MUST 同步提交可访问的最终视觉状态，不创建 active 动画或持续 spinner deadline

#### Scenario: reduced motion
- **WHEN** effective motion policy 为 reduced
- **THEN** 系统 MUST 使用与 `motion=false` 相同的非持续行为，同时保留状态变化、loading 语义和最终可见反馈

### Requirement: 最小 retained 失效
每个动画 target SHALL 明确声明允许的 dirty domain；基础动画只可更新 Material、Transform、Geometry 和 Animation，MUST NOT 默认触发 Structure、全树 remount、Measure 或 Layout，也不得上传目标之外的 scene range。

#### Scenario: Material 动画
- **WHEN** Button color 或 opacity 动画推进一帧
- **THEN** 系统 SHALL 只更新相关 Material/Animation state 和对应 GPU range，component identity、layout result、HitTest geometry 与无关 sibling 不得变化

#### Scenario: 非法 Structure target
- **WHEN** 基础动画尝试声明 Structure 或未授权的 Measure/Layout dirty domain
- **THEN** 创建或绑定 MUST 被拒绝并产生 diagnostics，不得在 tick 中隐式扩大失效范围

### Requirement: Button 首批 motion 消费
Button hover、active 与 loading 视觉 SHALL 通过统一 Runtime 消费 Theme motion specification；默认使用 `motionDurationMid` 与 `motionEaseInOut`，focus-visible outline SHALL 保持 `0s`，disabled、loading eligibility、pressed 和 pointer capture 语义 SHALL 同步生效而不等待视觉动画。

#### Scenario: hover 与 active 中断
- **WHEN** pointer 在 transition 完成前快速进入、按下、释放和离开 Button
- **THEN** 每次视觉 retarget SHALL 从当前采样状态连续过渡，最终状态符合最新 interaction state，且不得出现额外 focus ring

#### Scenario: keyboard focus
- **WHEN** Button 通过键盘获得或失去 focus-visible
- **THEN** focus outline SHALL 在当前 frame 立即切换，不使用 hover/active duration，也不得改变 interaction eligibility

#### Scenario: disabled 与 loading 优先级
- **WHEN** active Button 在动画期间切换为 disabled 或 loading
- **THEN** click eligibility、pressed 与 capture state MUST 立即收口，视觉可按 effective motion policy 过渡到最终 disabled/loading 状态

### Requirement: retained loading indicator
motion enabled 时，Button loading indicator SHALL 使用 retained spinner phase 并仅在可见且 active 时请求连续 frame；motion disabled 或 reduced 时 SHALL 显示稳定的静态 loading indicator，且两种模式均不得逐帧重建 scene structure。

#### Scenario: loading spinner 生命周期
- **WHEN** Button 从非 loading 切换为 loading，再切回非 loading
- **THEN** spinner SHALL 在 loading 生效时开始、在状态结束或 owner 销毁时移除 deadline，并保持 Button 与 scene identity 稳定

#### Scenario: 静态 reduced-motion indicator
- **WHEN** loading Button 的 effective motion policy 在运行时切换为 disabled 或 reduced
- **THEN** spinner SHALL 停止 phase 推进并保留可识别的静态 indicator，frame loop 在无其他请求时恢复 idle

### Requirement: diagnostics 与容量复用
Runtime SHALL 暴露可测试的 active、created、completed、canceled、retargeted、stale-operation、deadline、tick 和 skipped-cadence diagnostics；steady-state tick SHALL 复用预留容量，不得按帧产生 heap allocation。

#### Scenario: diagnostics 守恒
- **WHEN** 一组动画经历 create、retarget、finish、cancel、stale operation 和 owner cleanup
- **THEN** diagnostics SHALL 能按 identity 与总量解释所有生命周期结果，active count 最终为零且 completion 不得重复

#### Scenario: steady-state allocation
- **WHEN** 预热并预留容量后的固定动画集合连续运行 benchmark
- **THEN** tick path 的 heap allocation count MUST 为零，且 registry capacity 不得随帧数增长

### Requirement: 平台通用与平台专属验收分离
时间数学、插值、生命周期、Theme policy、Button state、dirty mapping、capacity 和 benchmark SHALL 作为平台通用合同只在一个受支持平台验收一次；依赖 platform monotonic clock、display cadence、window system、GPU/driver/shader、system font、input/DPI 或 packaging 的行为 SHALL 在 Windows 与 Linux 分别保留独立证据。

#### Scenario: 平台通用验收
- **WHEN** 平台通用测试在一个受支持平台和正式 preset 上通过
- **THEN** 另一平台 SHALL NOT 被要求重复相同逻辑合同，但证据 MUST 记录实际 OS、compiler、preset 和结果

#### Scenario: 平台专属验收
- **WHEN** change 准备完成 Windows 或 Linux checkbox
- **THEN** 对应平台 MUST 实际验证 monotonic clock、display cadence、真实窗口、GPU submit、DPI/input 和完成后 idle，且一方证据不得替代另一方
