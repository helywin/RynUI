## Purpose

为原生 Switch 保留锁定 Ant Design 6 的按压伸长反馈，并让手柄伸长与选中位置连续过渡，避免松开时瞬间缩回圆形；同时维持方向、尺寸、受控状态、运动策略与局部更新生命周期合同。

## ADDED Requirements

### Requirement: Switch handle press and release stay continuous

Switch SHALL 按 Ant Design 6.6.5 向轨道内侧伸长基础 handleSize 的30%，逻辑两端伸长与基础位置均按 motionDurationMid/ease-in-out 过渡。释放和中途 retarget MUST 从当前呈现几何衔接，不能先瞬间缩圆再移动；LTR/RTL与Middle/Small一致，阴影跟随同一几何。

#### Scenario: Release a held switch in either direction
- **WHEN** 鼠标或键盘按压伸长手柄后释放并切换 checked，或未完成过渡时再次按压/反向
- **THEN** 释放瞬间几何连续，后续宽度与位置同时变化，最终达到目标圆形与位置，内容不重挂或重测

### Requirement: Switch handle feedback follows motion and lifetime policy

手柄反馈 SHALL 使用现有动画 scope 与 geometry dirty 路径；reduced-motion/motion=false MUST 直接到目标并移除有限动画请求，disabled/loading/blur/失活取消按压目标，销毁清理动画与资源。

#### Scenario: Cancel or destroy during handle transition
- **WHEN** 手柄过渡期间改变运动策略、取消交互或销毁组件
- **THEN** 不残留伸长目标、失效回调或动画资源；结束后恢复 idle，受控值与实际激活语义保持
