## Why

RynUI 已有 motion Token、`DirtyFlags::Animation` 与按需 frame request，但没有动画时钟、插值、生命周期或 deadline 调度；Button hover/active、Theme 状态和 loading indicator 只能瞬时切换或保持静态。下一步需要建立统一的基础动画 Runtime，使组件能使用可测试、可取消、只在动画期间提交帧的 motion，而不各自实现 timer 和状态机。

## What Changes

- 增加 UI owner-thread `AnimationRuntime`、generation-checked `AnimationId`、确定性 `AnimationClock` 与 active/deadline diagnostics；动画销毁、slot reuse、回调重入和 Scope cleanup 不得访问 stale state。
- 增加 typed tween contract，首批支持 `float`、`Color`、logical geometry 与 transform value，提供 validated cubic-bezier、linear 和 Ant Design 6.5.0 easing preset；不接受 CSS transition string 或任意字符串 easing。
- 定义 delay、duration、play、finish、cancel、retarget 和 interruption 语义。retarget 从同一时间点的当前采样值连续过渡，cancel 保留当前值，finish 精确提交目标值。
- 把 Animation deadline 接入 `OnDemandFrameLoop`：活动动画按 nominal display cadence 唤醒并进入 frame pipeline，事件与动画 wake 合并；最后一个动画结束后立即恢复 blocking idle，不持续满帧提交。
- 将 Theme `motion`、`motionUnit`、`motionBase`、fast/mid/slow duration 与 easing Token 接到 typed `MotionPolicy`；`motion=false` 或 reduced-motion policy 直接提交可访问的最终状态，不创建持续 frame task。
- 将动画更新映射到 Material、Transform、Geometry 与 Animation dirty domain；首批不做逐帧 Structure rebuild，也不默认驱动 Measure/Layout。
- 首批接入 Button：hover/active/loading opacity/color 使用 Ant Design `motionDurationMid` + `motionEaseInOut`，focus-visible outline 保持上游 0s，disabled/interaction eligibility 同步收口；loading 使用 retained spinner phase，motion 关闭时显示静态 indicator。
- 增加 controlled clock、60/120/144 Hz cadence、overshoot easing、retarget/cancel/dispose、Theme motion toggle、Button state、GPU dirty range、allocation/benchmark 与真实窗口验收。
- 非目标：本 change 不发布稳定 consumer animation DSL，不实现 spring/physics、keyframe timeline、layout transition、shared-element、path morph、gesture-driven animation、wave/ripple 或任意 shader expression；这些进入后续复杂动画 change。

## Capabilities

### New Capabilities

- `animation-runtime`: 定义基础 Animation clock、typed tween/easing、generation 生命周期、deadline frame scheduling、motion policy、dirty mapping、Button 首批消费和跨平台验收合同。

### Modified Capabilities

无。当前 `openspec/specs/` 尚无已归档 animation capability；既有 Reactive、Theme、Button 与 Application Runtime 的稳定公开合同不在本 change 中改名或移除。

## Impact

- 新增 `src/animation/` 内部模块并扩展 `src/runtime/frame_scheduler.*`、Theme motion adapter、Button component/scene、examples 和 tests；不从 `rynui.hpp` 导出新的稳定动画 API。
- 不新增第三方依赖。Ant Design 6.5.0 motion 默认值继续来自锁定 Token snapshot：fast/mid/slow 为 0.1s/0.2s/0.3s，easing 使用 typed cubic-bezier value，`motion=false` 将 duration 归零。
- 正式构建继续通过 CMakePresets 和 Ninja Multi-Config；平台通用时间/插值/生命周期只验收一次，Windows 与 Linux 分别验证 monotonic platform clock、display cadence、真实 GPU、DPI 与窗口 idle 行为。
- 主要风险是 frame deadline 造成 busy loop、retarget 产生数值跳变、overshoot easing 被错误 clamp、动画 callback 销毁组件后访问 stale identity、逐帧 dirty 扩大到 Layout/Structure，以及 reduced motion 仍保留持续 spinner。
- 可验证结果是 Button 状态在 motion enabled 时平滑且可中断，motion disabled/reduced 时同步到最终视觉；动画期间只上传目标 ranges，完成后 frame loop 恢复 idle，controlled clock 下跨运行结果确定。
