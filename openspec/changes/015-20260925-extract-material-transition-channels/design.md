# Design

## Context

Button 的 `register_animation_targets`、`retarget_channel`、`unregister_animation_targets` 与 Input 的 `InputMaterialTransition` 均建立带 generation 的 scope/target，再在 Material/Animation domain 内原地 retarget。Button 的 spinner 会重复播放并在完成时重启；Input 的颜色与 shadow opacity 是有限持续时间的过渡。这些策略不同，底层 target 生命周期和 retarget 的三种分支相同。

## Goals / Non-Goals

**Goals:** 固定通道数、强类型 `AnimationValueKind`、异常安全 scope 释放；共享“当前值等于目标时收敛、活动动画原地 retarget、无活动动画 play”的实现；保持没有活动动画时无下一帧 deadline。

**Non-Goals:** 不引入公开动画 DSL、通用样式 Modifier、统一 Token/Focus/Spinner 表现，也不改 retained surface 服务或平台 GPU adapter。

## Decisions

### 1. 内部 target group 只负责生命周期

`MaterialTransitionTargets<N>` 构造时接受现有 `AnimationRuntime`、sink、每通道 typed kind 和显式 dirty domain，建立一个 scope 并注册 N 个 target。注册失败释放整个 scope；析构释放 scope，且不可复制/移动。Button 每个挂载状态持有一个 group，Input 的过渡对象内嵌一个 group。组件仍拥有当前显示值、animation id、目标值与完成回调，group 不知道任何 Button/Input Token。

### 2. 只共享有限过渡的 retarget 分支

`retarget_material_channel` 接受 runtime、target、当前 presentation、期望值、active id、spec 和时间，沿用现有 Button/Input 规则：相等目标使活动动画以零持续时间收敛；否则对活动动画 retarget 或新建动画。Input 的 same-spec/同目标跳过由其调用点保留；Button spinner 的循环播放、取消及完成重启仍在 Button 内，避免把无限动画混入有限材质过渡。

### 3. 保留组件 dirty 声明与 generation 安全

两个 consumer 注册时都显式传 Material|Animation；现有 sink 的 domain 检查继续保留。Button 的 target→component/channel binding 在 scope 销毁时移除；Input 的 callback 保留组件 generation 检查。theme 纯颜色更新只能进入 Material/Animation，不能触发 Measure、remount 或稳定 scene topology 重建。

## Validation

- helper 合同：typed color/scalar、多通道 scope 生命周期、同目标/中途反向 retarget、dispose 后无 deadline、构造失败回滚。
- Button：hover/press/loading、spinner/reduced motion、focus ring、回调销毁、idle allocation 与 scene dirty/upload。
- Input：hover/status/theme/reduced motion、CJK 编辑、caret/selection、idle allocation 与同窗 Search/Selection 回归。
- 平台通用合同在 Windows MSVC 正式 preset 执行一次；只有涉及真实 OS/GPU 的新增行为才设平台专项，本轮纯内部重构不新增重复 Linux 验收，既有 Linux 待验保持原状。
