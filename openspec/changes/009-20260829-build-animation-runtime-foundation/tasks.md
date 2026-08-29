## 1. 确定时间、typed value 与 easing

- [x] 1.1 在 `src/animation/` 增加 `AnimationTime`、`AnimationDuration`、production steady clock 与 controlled test clock，使用整数 microseconds 和绝对时间计算；通过同 timestamp、倒退 timestamp、large value、overflow boundary、exact start/end 和 deterministic replay tests 验证
- [x] 1.2 增加 closed `AnimationValue` 与 `float`/`Color`/`Point`/`Size`/`Rect`/`LogicalOffset` typed interpolation，校验 type match 与 finite input；通过 endpoint、component-wise geometry、sRGB Color saturation、mismatched type 和 invalid value tests 验证
- [x] 1.3 实现 linear 与 hybrid Newton/bisection cubic-bezier solver，锁定 Ant Design 6.5.0 八组 easing preset；通过 x-domain validation、y overshoot、flat derivative、endpoint、reference samples 和 source contract tests 验证
- [x] 1.4 将 animation sources/tests 接入 CMake target，使用一个受支持正式 preset 运行本组 unit/source/public forbidden-include tests 和 `git diff --check`；以英文 `feat: add animation timing primitives` 提交并推送本阶段，核对 remote SHA

## 2. generation-safe AnimationRuntime 生命周期

- [x] 2.1 实现 `AnimationId`、`AnimationScopeId`、`AnimationTargetId` 的 slot + generation registry、free list、owner-thread guard、reserve 与 stale rejection；通过 invalid id、destroy/reuse、Scope cleanup、wrong-thread 和 capacity reuse tests 验证
- [x] 2.2 实现 target sink、allowed `AnimationDirtyDomain` 与 typed create/play record，拒绝 Structure、Measure/Layout 和 type mismatch；通过 target unregister、owner dispose、Material/Transform/Geometry mapping 和 stale sink tests 验证
- [x] 2.3 实现 delay、duration、zero-duration、tick、cancel、finish、原地 retarget 与 interruption，callback 后重新解析 identity；通过 callback self/sibling destroy、create during tick、once-only completion、cancel current value、finish endpoint 和 no-jump retarget tests 验证
- [x] 2.4 增加 lifecycle diagnostics 与预留 tick snapshot benchmark，证明预热后 steady-state tick/cancel/finish/retarget 无 heap allocation、capacity 不随帧增长且 counters 守恒
- [x] 2.5 运行 AnimationRuntime lifecycle、target、dirty、allocation、owner-thread 和 `git diff --check`；以英文 `feat: add animation runtime lifecycle` 提交并推送本阶段，核对 remote SHA

## 3. 按需 frame deadline 调度

- [x] 3.1 为 `OnDemandFrameLoop` 增加可选 `FrameDeadlineSource` 和统一高精度 monotonic timestamp，在没有 deadline 时保持现有 one-shot request 行为；通过 no-source compatibility、event-only、timeout rounding 和 idle tests 验证
- [x] 3.2 将 AnimationRuntime 最早 deadline 接入 frame loop，按 nominal display cadence 合并 input/resize/animation wake并跳过 missed cadence；通过 60/120/144 Hz、event+deadline coalescing、long stall、no catch-up queue 和 multiple-animation earliest-deadline tests 验证
- [x] 3.3 将同一 frame timestamp 接入 animation tick、invalidation、scene/GPU 与 submit pipeline，deferred submit 保留 dirty state且不得在同 timestamp 重复推进或 completion；通过 deferred/retry/failure、exact endpoint、range persistence 和 last-animation idle tests 验证
- [x] 3.4 扩展 frame diagnostics 记录 deadline wake、coalesced wake、animation frame、missed cadence 与 idle-after-animation，并以 controlled-clock frame journey 证明动画结束后不再 poll/submit
- [x] 3.5 运行 frame scheduler、frame renderer、application/runtime integration、idle benchmark 与 `git diff --check`；以英文 `feat: schedule animation frame deadlines` 提交并推送本阶段，核对 remote SHA

## 4. Ant Design MotionPolicy 与 Theme 接入

- [x] 4.1 增加 internal `MotionTokenSet`，从 Theme `motionBase + motionUnit × 1/2/3` 派生 fast/mid/slow 并集中保存八组 typed easing；通过默认 `0.1s/0.2s/0.3s`、override、algorithm/nested Theme 和 Ant Design 6.5.0 source contract tests 验证
- [x] 4.2 增加可注入 effective `MotionPolicy`，组合 Theme `motion` 与 `normal|reduced` preference；通过 motion disabled/reduced duration 归零、runtime policy toggle、final state、无 active record 和无 spinner deadline tests 验证
- [x] 4.3 将 motion Token identity 映射到 Animation dirty phase并更新 Token diagnostics/generated reference；通过 Theme notification isolation 证明 motion 更新不触发 Measure/Layout、Text shaping 或无关 component subscriber
- [x] 4.4 增加 public dependency/API contract，证明 Runtime、target、clock 和 MotionPolicy 不从 `rynui.hpp` 导出、不新增通用 Modifier 或字符串 easing；运行 Theme/Token/runtime tests 与 `git diff --check`
- [x] 4.5 以英文 `feat: add theme motion policy` 提交并推送本阶段，核对 remote SHA

## 5. Button 状态视觉动画

- [x] 5.1 将 `ButtonComponentState` 拆分为同步 semantic/interaction state 与 retained presentation state，为 background、border、foreground 和 loading opacity 注册 generation-checked target；通过 mount/destroy/reuse、Theme update 和 current-state snapshot tests 验证
- [x] 5.2 将 hover、active、loading 与 disabled presentation 接到 `motionDurationMid` + `motionEaseInOut`，每 channel 原地 retarget；通过快速 enter/down/up/leave、Theme switch、disabled/loading priority、exact final color/opacity 和无数值跳变 tests 验证
- [x] 5.3 保持 focus-visible outline `0s` 并让 click eligibility、pressed、keyboard state 与 pointer capture 同步收口；通过 pointer focus 无 ring、keyboard focus instant ring、animation 中 disable/loading、capture cancel 和 click suppression tests 验证
- [x] 5.4 证明每 tick 只更新 Button 目标 Material/Animation range，不改变 Component/Node/Interaction/fragment identity，不产生 Structure/Measure/Layout/HitTest 或 sibling upload；扩展 CPU/GPU reference 与 diagnostics tests
- [x] 5.5 运行 Button component/scene/input/focus/Theme、GPU range、public API、frame integration 和 `git diff --check`；以英文 `feat: animate button state visuals` 提交并推送本阶段，核对 remote SHA

## 6. retained Button loading spinner

- [x] 6.1 将 static loading layer 改为固定八段 retained rounded-quad topology，非 loading 时隐藏但不增删 fragment layer；通过 geometry、DPI scale、clip、scene order、mount/destroy/reuse 和 topology stability tests 验证
- [x] 6.2 实现 linear spinner phase，仅在 loading 可见且 motion enabled 时更新八段 Material opacity；通过 phase wrap、cadence-independent endpoint、multiple Button、visibility、owner dispose 和 exact dirty-range tests 验证
- [x] 6.3 为 motion disabled/reduced 提供可识别静态 indicator，并在运行时 policy/loading 切换后移除 deadline；通过 no continuous tick/submit、idle recovery、final loading semantics 和 Theme toggle tests 验证
- [x] 6.4 运行 loading spinner、Button layout/scene/GPU、frame idle、capacity benchmark 与 `git diff --check`；以英文 `feat: add retained button spinner` 提交并推送本阶段，核对 remote SHA

## 7. 平台通用集成与架构基线

- [x] 7.1 增加完整 headless journey，覆盖 controlled clock、60/120/144 Hz、hover/active/loading/focus、rapid retarget、Theme motion toggle、reduced policy、GPU deferred retry、owner destroy、最后一帧与恢复 idle；验证逐帧 presentation、identity、dirty range 和 diagnostics
- [x] 7.2 增加 animation evidence schema，要求 clock/cadence、Theme source/token、lifecycle counters、allocation、Button state、target/upload range、frame submit/idle、preset/compiler/platform/driver/shader/font 与真实窗口 evidence path；拒绝 planning-only、跨平台 identity 和缺失 completion/idle 证据
- [x] 7.3 更新 `docs/architecture.md` 与必要的 generated Token reference，明确基础 Runtime、复杂动画后续边界、Button 首批消费和 reduced policy seam；README 仅在项目状态或文档入口确有变化时更新，AGENTS 工作流不得混入 README
- [x] 7.4 在一个受支持平台记录实际 OS/compiler/preset，运行全部平台通用 animation/runtime/Button unit、headless、contract、benchmark、public dependency、lock/license 和 Python cache 检查；确认 tracked/untracked 均无 `__pycache__`、`.pyc`、`.pyo`
- [x] 7.5 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`；以英文 `test: validate animation runtime contracts` 提交并推送平台通用 evidence，不要求另一平台重复本组合同，核对 remote SHA

## 8. Windows 专属验收

- [x] 8.1 使用 `windows-msvc` preset clean configure，完成受 Windows 分支影响的 Debug/Release build 与 CTest，核对 Ninja Multi-Config、MSVC x64、steady clock/display cadence adapter、BUNDLED 依赖、D3D12/DXIL 与 evidence identity，保存独立 Windows 结果
- [ ] 8.2 在 Windows/MSVC/D3D12/DXIL 真实窗口以系统 display scale 和 1.0/1.25/1.5/2.0 acceptance render scale 操作 Button enter/down/up/leave、keyboard focus、loading、Theme motion on/off 与 injected reduced policy；记录 nominal refresh、driver、font、scale、退出码、frame/animation diagnostics 和 evidence path
- [ ] 8.3 人工核对视觉过渡连续、focus ring 瞬时且不与 hover 蓝边混淆、spinner retained、motion disabled/reduced 为静态 indicator、DPI 下无裁切或抖动；等待动画完成后确认窗口无持续 submit/busy loop
- [ ] 8.4 运行 Windows 平台分支 tests、evidence passed contract、dependency/shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows animation runtime` 提交并推送 Windows evidence，核对 remote SHA，不修改 Linux 清单

## 9. Linux 专属验收

- [ ] 9.1 使用 `linux-gcc` clean configure 和 `linux-clang` Debug build，完成受 Linux 分支影响的 CTest，核对 Ninja Multi-Config、标准 C++20、steady clock/display cadence adapter、BUNDLED Wayland/libdecor、Vulkan/SPIR-V 与 evidence identity，保存独立 Linux 结果
- [ ] 9.2 在原生 Linux Wayland/GCC/Vulkan/SPIR-V 真实窗口以至少两档实际 display scale 操作 Button enter/down/up/leave、keyboard focus、loading、Theme motion on/off 与 injected reduced policy；记录 compositor、nominal refresh、driver、font、scale、退出码、frame/animation diagnostics 和 evidence path，不以 X11 代替
- [ ] 9.3 人工核对视觉过渡连续、focus ring 瞬时、spinner retained、motion disabled/reduced 静态状态、不同 refresh 下稳定且 DPI 无裁切或抖动；等待动画完成后确认窗口无持续 submit/busy loop
- [ ] 9.4 运行 Linux 平台分支 tests、evidence passed contract、dependency/shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux animation runtime` 提交并推送 Linux evidence，核对 remote SHA，不修改 Windows 清单

## 10. Change 收口

- [ ] 10.1 在准备 archive 时运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、最终受影响 CTest/benchmark、public dependency、lock/license/cache、`git diff --check`、remote SHA 与 clean worktree 检查；确认平台通用、Windows 与 Linux checkbox/evidence 各自真实完成，README、AGENTS、architecture、generated docs 与 OpenSpec 职责未混写，本项不替代任何平台验收
