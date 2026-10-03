# RadioButton 填充与 Switch 手柄连续过渡

2026-10-03，平台通用合同在实际 Windows / MSVC / Ninja Multi-Config / `windows-msvc-headless` Debug、Release 验证。分平台 GPU 证据单独记录，不以 headless 结果代替窗口与 shader 验收。

## 修正

- RadioButton 旧背景止于边框内侧，两层互补 AA 按 straight-alpha 叠加时会露底。背景改为完整 border box，同色边框透明占位，只绘制一次外缘。选中前项拥有共享边，后项背景与边框裁到边外，移除额外 seam 覆盖但保留原 range capacity/identity。
- Switch 旧宽度由按压布尔值立即切换，仅位置有动画。新增逻辑 start/end 两个 scalar 通道，基础位置、向内30%伸长及释放收缩均按 motionDurationMid 与 CSS ease-in-out 过渡。锁定官方使用的是字面量曲线 `(0.42,0,0.58,1)`，区别于 Ant motionEaseInOut token `(0.645,0.045,0.355,1)`。
- 只更新 retained geometry/material 和手柄阴影。reduced-motion/motion=false 同步到目标；取消与销毁清理通道，既有 public API、组件回调、内容身份保持。

## 回归

`switch_features` 检查16组尺寸/方向/初值/鼠标或键盘组合：按下0/50/200ms，释放瞬间几何完全相同，50ms宽度和位置同时变化，快速重按与反向，最终回到圆形/idle；检查 CSS 25% 时间进度、reduced同步、销毁、受控 checked 在按压中变更、disabled/loading/blur/失活/motion-off取消，且内容不重挂或重测。

`radio_features` 对浅/深、LTR/RTL、横向/竖直连接组合，在1/1.25/1.5/2物理采样尺度对实际有序 scene 做 straight-alpha 合成，检查完整内部（包含共享边与原内侧边框曲线）保持选中颜色，并保留原选中共享边优先级、动态成员、token与生命周期回归。

`checkbox_group` 的销毁检查记录挂载时实际 target 数量并要求只移除自身 wave target，避免将兄弟 Switch 的私有通道数量硬编码为3；验证的资源隔离合同保持。

## 验证记录

完整CTest包含allocation、renderer contract、Core boundary与所有组件回归；Debug 99/99（179.83s）、Release 99/99（30.14s），详见 [Debug日志](selection-debug.log)、[Release日志](selection-release.log)。格式化、OpenSpec doctor/56项strict validate与diff check在提交前完成。后续窗口中间帧、填充逐像素比较见 [Windows补充证据](../windows-feedback/README.md)；Linux原生项仍独立待完成。
