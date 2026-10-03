# Design

## Context

动机见 proposal。`quad.hlsl` 已按导数平滑圆角，outline 已使用 packed physical AA width=1；`rounded_effect.hlsl` 的 GaussianEdge 在 sigma=0 使用 0/1，inset 对 surfaceDistance 硬 clip。`make_corner_fill_effects` 与常规组件 surface 均借用零 blur outer effect，因此共享 shader 会覆盖原本平滑的 Quad。Core/store 是 logical scene v3，packed ABI v2 的 effect 仍为 112 bytes。

Button 当前只有一个 wave_progress 通道，以 slow/ease_out 同时更新固定厚度 ring 的 offset 和 opacity；默认 wave_width=2、wave_spread=6。原生接口与 retained effect store 已有取消/重启/清理合同。

## Goals / Non-Goals

Goals：修正共同 coverage、RadioButton 填充合成和 Button/Switch 官方反馈；一个物理像素的 AA 不能随 logical DPI 放大，动画只改既有 geometry/material，独立 fade 不提前移除 wave。具体范围与非目标见 proposal。

## Decisions

### 圆角覆盖

零 blur 使用 SDF 的 `1-smoothstep(-AA/2,+AA/2,d)`，blur>0 保留 Gaussian 衰减；inset 的 base surface mask 使用同一 coverage 相乘，移除硬 surface clip，并扩展 draw bounds 的 AA guard。祖先 clip 仍是硬裁剪，四象限边界由既有 geometry/scissor 划分，不把每个 quadrant 当作需 feather 的形状。同步 logical reference、packed shader reference、HLSL 与 source SHA；不改 ABI、不引入 MSAA 或 renderer 类型到 Core。新增边界/曲面/平移/fractional DPI/四象限 seam 回归与真实 GPU 像素检查，Quad/outline 在相同窗口作为对照。

### Button wave

按已核对的官方 6.6.5 源码：外扩 box-shadow 无 blur，400ms 达到 spread=6，2000ms 达到 opacity=0，两者使用 theme motionEaseOutCirc（默认 cubic-bezier(0.08,0.82,0.17,1)）。增加独立 wave_fade 通道，wave_progress 保留扩展意义；扩展通道完成后继续 fade，只有 fade 完成或策略取消移除 retained effect。官方 quick 仅 Checkbox/Radio，本 change 不改其合同。

使用既有 outline kind 表达外侧色带：outer_extent=wave_spread×progress，width=min(wave_width,wave_spread)×progress，offset=outer_extent-width。默认 width/spread 都是6，因此紧邻按钮轮廓而无空隙；0进度隐藏 effect（合法 outline width 必须正），不制造 epsilon 几何。两通道都取消才清理；重复 activation 仍重启一个 effect。wave width/spread 为0时无波纹；这些原生扩展参数不是官方 Design Token，文档明确最终几何语义。默认颜色优先实际 presentation border，透明/白色时尝试 background，再 theme primary；不固定 Default 为蓝色。

短时 ease_out + 固定厚度游离 ring 无法表达官方 box-shadow，不保留为默认。依赖 DOM/浏览器或自定义顶点路径都不必要。官方 raf 对尺寸同步的职责由原生 retained layout 后发布承担；无须复制 React。

### RadioButton 连续填充

旧实现将背景裁到内侧边界，边框在同一边界取互补覆盖。两者按 straight-alpha 分别叠加时，0.5与0.5合成只有0.75覆盖，会露出底色，形成偏移浅线。背景改为覆盖完整 border box，异色边框绘制在背景之上；同色边框隐藏，仅用一层背景表达完整外形，避免外侧 AA 重叠加深。选中前项拥有共享边，后一未选中项的背景与边框裁到该边之外，避免先覆盖蓝边再用单独 AA 色带补回所产生的浅线；保留第九个透明占位以维持 range identity/capacity。四象限与连接边优先级、retained range 保持，零尺寸合法。测试取实际 scene 的填充/边框合成，检查全部内部颜色连续（含共享边）及混合圆角。

### Switch 手柄连续按压反馈

官方6.6.5的 handle 基础位置与 ::before 两个逻辑 inset 均使用 motionDurationMid 与 CSS ease-in-out（cubic-bezier(0.42,0,0.58,1)，区别于 motionEaseInOut token），active 将向轨道内侧的 inset 设为-30%。旧实现宽度按 pressed 布尔值立即变化，而位置独立动画，故释放时瞬间缩回圆形后才移动。保留基础手柄锚点，新增逻辑 start/end 两个0..1动画值，分别乘 handleSize×30%；unchecked 向 end 拉长、checked 向 start 拉长，RTL镜像。释放将两端目标归零，与 checked 位置通道同时过渡；中途重按、受控 checked 更新从当前呈现值 retarget。伸长限制在轨道合法宽度内，手柄阴影随同一几何更新；loading 图标仍以基础手柄中心定位。

只更改 retained geometry，不重挂或重测内容。普通策略按压与释放都动画；reduced-motion/motion=false 直接到目标值并清理通道，disabled/loading/blur/失活取消按压目标，销毁清理整个 scope。平台通用测试覆盖两种尺寸、LTR/RTL、双向切换、快速重按、鼠标/键盘、策略/生命周期与 idle；Windows 实际窗口捕获按压/释放中间帧和 RadioButton 填充，Linux 独立记录。

## Risks / Trade-offs

- [边缘变淡或出现漏色] → 一像素 AA、真实浅/深背景、细 border 与 fill 叠层、四象限 seam、ancestor clip 对照；校验实际像素不是仅 source 字串。
- [旧 wave override 视觉变化] → 保留公开接口，明确 width 最终厚度/spread 最终外边缘，更新测试和文档。
- [2秒尾部保留动画请求] → 与官方 fade 时间一致，400ms 后不重复扩展；2秒/失活/disabled/loading/reduced/motion-off/dispose 后无 wave deadline。
- [CPU/GPU 或平台不一致] → 两份 reference 与统一 HLSL 锁同步，Windows 实际 D3D12/DXIL 数值/readback，Linux Vulkan/SPIR-V 待实际机器独立验收。

## Migration Plan

规划提交 → 共同 AA 与 reference/合同验证提交 → Button wave 与完整 headless/相关 Gallery 验证提交 → Windows 独立 evidence 提交。全部使用 presets/Ninja Multi-Config/MSVC；不推送或 archive 新 change，除非用户要求。回退对应提交恢复旧视觉，不涉及存储迁移。

## References

2026-10-03核对官方版本6.6.5：[wave/style.ts](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/style.ts)、[WaveEffect.tsx](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/WaveEffect.tsx)、[util.ts](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/util.ts)、[Button.tsx](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/Button.tsx)、[Switch style/index.ts](https://github.com/ant-design/ant-design/blob/6.6.5/components/switch/style/index.ts)、[seed.ts](https://github.com/ant-design/ant-design/blob/6.6.5/components/theme/themes/seed.ts)。官方参考只证明设计，验收使用本 change 的实际测试与 GPU 证据。
