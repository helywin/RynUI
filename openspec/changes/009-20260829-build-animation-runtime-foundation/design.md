## Context

见 `proposal.md` 的动机与范围，行为合同见 `specs/animation-runtime/spec.md`。当前 `OnDemandFrameLoop` 只有 one-shot `FrameRequestState` 与固定毫秒 idle wait，Theme 仅保存 `motionUnit`、`motionBase` 和 `motion`，Button 在 interaction state 变化时直接重算最终 scene value；`ButtonSceneService`、Component/Node/Interaction identity 已采用 slot + generation，并能分别更新 Material、Geometry 和 RoundedEffect。

现有 `Color` 只接受 `[0, 1]` sRGB channel，`Duration` 使用有限非负毫秒值，`CubicBezier` 已验证 x control point；这些公开值可作为 Theme 输入，但 frame timing 不能继续使用浮点毫秒累计，否则 120/144 Hz cadence、retarget 和长时间运行会积累误差。

本 change 不新增第三方库。Ant Design `6.5.0` 的 motion seed 固定 `motionUnit=0.1s`、`motionBase=0s`，fast/mid/slow 由 base + unit × 1/2/3 派生，`motion=false` 时 duration 为 `0s`；Button transition 使用 mid/easeInOut，focus-visible transition 为 `0s`。

## Goals / Non-Goals

**Goals:**

- 建立一个 owner-thread、确定时间、generation-safe 且 steady-state tick 无 heap allocation 的内部 Runtime。
- 让 frame loop 依据最早 animation deadline 阻塞等待，而不是使用持续 timer 或固定满帧循环。
- 让 Theme motion Token、Button state 与 scene dirty range 通过 typed adapter 相连，interaction eligibility 与视觉过渡解耦。
- 为未来 Scroll、Clip 和更多组件保留稳定的内部 clock、easing、lifecycle 与 target extension point。

**Non-Goals:**

- 不从 `include/ryn/rynui.hpp` 导出 `animate()`、timeline、public handle 或 Compose-style animation DSL。
- 不支持 spring、keyframe、layout/shared-element、path、gesture、wave/ripple 或 shader expression。
- 不自动动画化 `Prop<T>`、LayoutStyle、Theme snapshot 或任意 dirty value；每个消费方必须显式声明 target 与 dirty domain。
- 不在本 change 引入 Linux desktop settings/DBus 依赖；reduced motion 先通过可注入的 effective policy 进入 Runtime，自动发现系统偏好另建平台 capability。

## Decisions

### 1. 内部时间统一为整数 microseconds

`src/animation/time.*` 定义强类型 `AnimationTime` 与 `AnimationDuration`，底层为有符号 64-bit microseconds；production clock 从 `std::chrono::steady_clock` 取得 monotonic time，test clock 显式 set/advance。Theme 的 `ryn::Duration` 在创建 specification 时一次性校验并转换，不在 tick 中累计浮点 delta。

Runtime 按绝对 `start + delay + duration` 计算 normalized progress；missed cadence 只让下一次 tick 采样当前绝对时间，不追赶历史 step。同一 timestamp 是幂等采样，倒退 timestamp clamp 到 last observed time 并计数。结束点显式写入 target value，避免 cubic solver 的浮点近似破坏 exact endpoint。

备选方案是直接传 `double seconds` 或每帧累加 delta。前者缺少单位边界，后者使不同 refresh rate 得到不同完成时间和误差，因此不采用。备选方案是 nanoseconds；当前 UI motion 以毫秒 Token 和 display cadence 为主，microseconds 已足够且乘除溢出边界更宽。

### 2. Runtime 使用 slot + generation registry 和预留 tick snapshot

`AnimationRuntime` 保存 slot registry、free list、active identity list 与 reusable tick snapshot。`AnimationId` 包含 index/generation；`AnimationScopeId` 和 `AnimationTargetId` 同样通过 generation registry 解析，不在 animation record 中保存可越过 callback 的 owner raw pointer。

tick 开始时把当时的 active identity 复制到已预留 snapshot，逐项重新 `find(id)`；target apply、completion 或 lifecycle callback 返回后再次解析 identity。callback 可以 cancel/finish/retarget、自毁、销毁 sibling 或创建新动画；本 tick 新建的动画从下一 tick 开始，已销毁 identity 会自然跳过。Scope dispose 先使 target identity 失效，再取消其动画。

`reserve(animation_capacity, target_capacity)` 是 host 初始化的一部分；create 路径可以在容量不足时增长，但预热并预留后的 tick、cancel、finish 和 retarget path 不分配。diagnostics 区分 stale request、non-monotonic time、callback mutation 与 capacity growth。

备选方案是每个组件持有 `std::function` timer callback。它容易产生 owner dangling、按帧分配和组件各自调度。备选方案是 intrusive pointer shared ownership；它会延长已销毁组件生命周期并掩盖 stale identity，因此不采用。

### 3. Tween payload 使用 closed typed variant，target 通过 generation-checked sink 写入

首批 `AnimationValue` 是 closed `std::variant`，包含 `float`、`Color`、`runtime::Point`、`runtime::Size`、`runtime::Rect` 与 `LogicalOffset`；translation 使用 `runtime::Point`，不另造矩阵或任意 vector type。创建时 `from`、`to` 与 target kind 必须匹配，所有 scalar 必须 finite。

每个 target sink 在 Runtime 注册并获得 generation-checked identity；sink 接收 `AnimationValue` 和固定 `AnimationDirtyDomain`，再调用目标 service 的 typed update。Runtime 不认识 Button、Node 或 GPU range，但会拒绝 Structure、Measure/Layout target。sink 回调期间可以触发 owner teardown，Runtime 依照决策 2 重新验证。

`float` 与 geometry 做 component-wise interpolation。`Color` 按现有 Color storage 在 sRGB channel space 插值，并在构造合法 `Color` 前 saturate channel 到 `[0, 1]`；easing solver 自身保留 overshoot，因此 float/geometry target 可按合同观察 overshoot，Color 的值域约束由 type adapter 收口。未来若引入 linear-light color，必须单独修改 Token/rendering contract。

备选方案是 template 化整个 registry；它会复制生命周期与调度逻辑并使统一 diagnostics 困难。备选方案是 string/property path；它违背 typed API 与编译期边界，因此不采用。

### 4. Cubic-bezier 以 hybrid solver 计算，easing 与目标 clamp 分离

`Easing` 是 `linear` 或 typed `CubicBezier`。normalized time 先 clamp 到 `[0, 1]`；cubic solver 对 `x(t)=progress` 先使用有界 Newton-Raphson，在斜率过小或未收敛时回退二分，最后计算 `y(t)`。x control point 继续要求 `[0,1]`，y 可超出范围以支持 Back preset。

Ant Design preset 由单一 internal `MotionTokenSet` 生成并做 source contract：`easeOutCirc=(0.08,0.82,0.17,1)`、`easeInOutCirc=(0.78,0.14,0.15,0.86)`、`easeOut=(0.215,0.61,0.355,1)`、`easeInOut=(0.645,0.045,0.355,1)`、`easeOutBack=(0.12,0.4,0.29,1.46)`、`easeInBack=(0.71,-0.46,0.88,0.6)`、`easeInQuint=(0.755,0.05,0.855,0.06)`、`easeOutQuint=(0.23,1,0.32,1)`。组件只引用 typed identity，不复制 control point。

备选方案是 lookup table；它引入分辨率与 endpoint 误差。备选方案是把输出统一 clamp 到 `[0,1]`；它会破坏合法 overshoot easing，因此不采用。

### 5. lifecycle record 原地 retarget，不链式创建动画

record 保存 `from`、`to`、timing specification、start time、last sampled value、state 与 target identity。retarget 先以同一 effective time 采样旧 record，把结果作为新 `from`，替换 `to/spec/start` 并递增 retarget diagnostics；identity 保持不变，避免快速 hover/active 产生 slot churn。delay 从 retarget time 重新开始。

cancel 先采样当前值再移除，不发 completion；finish 跳过 delay 并精确提交目标，只发一次 completion；零 duration 与 motion disabled 直接 apply target 和 completion，不进入 active list。destroy/Scope cleanup 等价于无回调 cancel，防止 teardown 中回调用户状态。

备选方案是每次 retarget cancel + create。它会改变 identity、难以证明 callback once，并在高频 pointer state 下扩大 free-list churn，因此不采用。

### 6. frame loop 接收可选 deadline source，并统一 frame timestamp

`OnDemandFrameLoop` 增加 `FrameDeadlineSource`，提供 `next_deadline()` 与 deadline-due consumption；event source 的 monotonic clock 提升到与 animation clock 一致的精度。step 先 poll event 和 due deadline，再合并到一个 `FrameRequestState`；若没有 pending request，则等待 `min(idle wait, time until earliest deadline)`。deadline 不是另一个 submitter，也不在后台线程推进动画。

frame pipeline 在 UI owner-thread 取得一次 frame timestamp，先 tick AnimationRuntime，再执行既有 invalidation/scene/GPU/submit。若 submit deferred，dirty range 保留；相同 timestamp 的 retry 不重复推进生命周期。Runtime 使用 display adapter 提供的 nominal refresh period，未知或非法值回退 60 Hz；60/120/144 Hz tests 固定检查 deadline 单调、missed cadence skip 和 endpoint 一致。

最后一个 active record 移除后 `next_deadline()` 返回空，frame loop 回到 blocking idle。`idle_wait_milliseconds` 继续作为 event integration 的最大等待片段，不再决定动画 cadence。

备选方案是独立 timer thread 每帧 `request_frame()`。它增加跨线程同步、shutdown race 和 coalescing 不确定性。备选方案是永远以 16ms wait 轮询；它在无动画时仍唤醒且不能正确覆盖 120/144 Hz，因此不采用。

### 7. effective MotionPolicy 由 Theme 与可注入偏好共同解析

internal `MotionPolicy` 包含 `enabled|reduced` 与 `MotionTokenSet`。Theme snapshot 解析 `motionBase + motionUnit × 1/2/3`，并保留命名 easing。effective preference 为 reduced 或 Theme `motion=false` 时，所有非必要 duration/delay 归零，spinner 进入 static presentation。

首批 host 提供 production default `normal` 和 test/application 注入 seam；不在本 change 把某个 Linux desktop 私有设置当成跨桌面标准，也不新增 DBus 依赖。Windows/Linux evidence 仍分别验证明确注入 reduced policy 后 idle 行为。未来平台偏好 source 只需改变 effective input，不改变 AnimationRuntime 或组件 target。

备选方案是每个 Button 读取 Theme `motion`。它会遗漏未来消费者、无法统一 reduced policy，并可能让 spinner 继续请求 deadline，因此不采用。

### 8. Button 分离 immediate semantic state 与 animated presentation state

`ButtonComponentState` 继续同步保存 disabled、loading、hovered、pressed、focus 和 capture eligibility；另增 presentation state 与有限的 animation identities。interaction resolver 先立即收口点击、pressed/capture 和 focus 语义，再根据最新 Theme/visual token retarget background、border、foreground/loading opacity。hover/active/loading 使用 mid/easeInOut；focus effect opacity 直接写入 scene，保持 0s。

loading indicator 改为固定八段 retained rounded quad：每个 Button 的 scene fragment 始终保留固定 segment topology，非 loading 时 opacity 为零；loading 时 linear phase 只更新八段 Material opacity，不旋转或重建结构。motion disabled/reduced 使用固定高低 opacity 分布作为静态 indicator。spinner phase 仅在 loading 可见且 owner live 时 active。

Theme 切换先解析新的最终 token，再从当前 presentation retarget；不把 Theme snapshot 本身动画化。Color、opacity 和 focus effect 更新走已有精确 Material range，固定 segment geometry 只在 Button geometry/layout 同步时更新。

备选方案是每 tick 重建 loading child Component 或改变 fragment layer count。它破坏 retained identity并触发 Structure compaction。备选方案是把 focus ring 一同过渡；Ant Design Button focus transition 为 0s，且这会再次混淆 hover border 与 keyboard focus，因此不采用。

### 9. diagnostics 与 evidence 以守恒关系验收

Runtime counters 至少记录 created/completed/canceled/retargeted/stale/non-monotonic/active/ticks/deadlines/due/missed-cadence/capacity-growth；frame loop补充 deadline wake/coalesced wake/animation frames/idle-after-animation。Button evidence 记录 interaction state、presentation value、animation identity、target range 与 spinner phase。

测试使用 controlled clock 覆盖 exact endpoints、callback 重入、slot reuse、60/120/144 Hz、GPU deferred retry、Theme toggle 和 rapid pointer journey。benchmark 预留容量后运行固定动画集合，断言 heap allocation 为零、capacity 不增长、无 Layout/Structure dirty、无 sibling upload。

平台通用合同只在一个正式 preset 验收一次。Windows 使用 `windows-msvc` + Ninja Multi-Config/MSVC/D3D12/DXIL；Linux 使用 `linux-gcc` 与 `linux-clang` + Ninja Multi-Config/Wayland/Vulkan/SPIR-V。只有 platform clock/display cadence、真实 window/GPU/DPI/input/system font 和 idle evidence 分别执行，本机路径仍只进入 ignored `CMakeUserPresets.json`。

## Risks / Trade-offs

- **[deadline wait rounding 导致 busy loop 或漏帧]** → 使用整数 microseconds、向上取整 wait、到期后按绝对时间计算下一 deadline，并以 wake/tick/idle counters 验证。
- **[callback teardown 形成 use-after-free]** → target/scope/animation 全部 generation checked，tick 使用 identity snapshot，任何外部调用后重新解析。
- **[Back easing 产生非法 Color]** → solver 保留 overshoot，Color adapter 单独 saturate channel；geometry/float 按 target contract决定可接受值域。
- **[固定八段 spinner 增加每个 Button instance 数量]** → topology 固定且容量可预测，用 batch upload/benchmark 衡量；若真实 Gallery 容量不可接受，回退为单段 retained indicator 而不改变 Runtime。
- **[Theme 切换中 presentation 与最新 interaction state 竞争]** → semantic state 是唯一 target resolver 输入，同一 owner 每个 channel 只保留一个 animation identity并原地 retarget。
- **[reduced motion 缺少统一 Linux 系统来源]** → 本 change 明确只接受 effective policy，默认 normal、支持显式注入；不得伪称已自动遵循所有桌面设置。
- **[内部 API 过早变成事实公共 ABI]** → 文件保留在 `src/animation/`，public header/dependency contract 拒绝从 `rynui.hpp` 导出，正式 consumer DSL 另建 change。

## Migration Plan

1. 先加入 time/easing/value 与 deterministic tests，不接入 frame loop 或组件。
2. 加入 generation registry、target/scope lifecycle 与 controlled-clock benchmark，保持现有 UI 行为不变。
3. 扩展 frame deadline、pipeline timestamp 和 MotionPolicy，证明无消费者时行为与现有 one-shot loop 等价。
4. 先把 Button immediate state resolver 接到 presentation target，再启用 color/opacity transition；focus 继续同步。
5. 最后替换 static loading quad 为固定 retained segments，并完成平台通用、Windows、Linux evidence。

每一步独立提交并可回退。若 deadline 集成异常，可禁用 Button consumer 并让 MotionPolicy duration 归零，frame loop 回到 one-shot request；若 spinner 性能不合格，可保留静态 retained indicator，而不删除已验收的 Runtime/time/lifecycle 基础。
