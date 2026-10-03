# Proposal

## Why

用户在 Windows 的 TextArea 等圆角矩形看到明显阶梯边缘。共享 rounded effect shader 的零 blur coverage 与 inset surface clip 使用硬阈值，而多个组件以零 blur effect 填充；问题并非限定 Windows。Button 的短时细空心环也与锁定 Ant Design 6.6.5 的外扩 box-shadow、独立扩展/淡出时间不一致。

## What Changes

- 为共同圆角 effect 的零 blur 边缘、inset shape mask 提供物理像素抗锯齿，并保持 CPU logical reference / renderer packed reference / HLSL 一致；检查既有 Quad、outline 的抗锯齿路径，保留硬 ancestor clip 与共享 GPU ABI。
- Button 默认 wave 改为沿真实轮廓扩展的外侧色带，0→6 logical px / 400ms，opacity 0.2→0 / 2000ms，分别使用 motionEaseOutCirc；颜色按可见 border、background、theme primary 回退。保留 reactive 开关、typed token override、布局/焦点/生命周期和 reduced-motion 合同。
- **BREAKING**：修正 Button wave 默认视觉与时序，wave_width 默认改为最终外扩色带的最大厚度 6，override 定义为随扩展增长的最终厚度（受 wave_spread 限制），不再是固定厚度的游离细环；公共类型与方法保持。
- 增加共同数学/packed/retained/lifecycle 回归，Windows 独立真实 shader/GPU 截图与数值检查；Linux 独立验收项不以 Windows 结果代替。

## Capabilities

`openspec list --specs` 当前为空，尚未同步主规格。沿用 006/044 的既有 delta capability 路径，追加以下明确合同，不创建近义能力：

### New Capabilities

- `shadow-rendering`：追加零 blur 圆角覆盖与 inset mask 的像素抗锯齿合同。
- `button-native-variants`：追加锁定官方 wave 的形状、独立时序、颜色和 retained 生命周期合同。

### Modified Capabilities

无已同步的主规格。

## Impact

`graphics/rounded_effect`、renderer/common reference、`rounded_effect.hlsl` 与 source lock；Button component 的独立动画通道和 Theme token 默认；相应 tests、Gallery 专用 acceptance、渲染/按钮参考文档。不增加第三方依赖、MSAA 开关或组件私有上传；Core 保持平台/renderer 隔离。非目标为其他控件 wave 时序、Happy Work 特效、Web DOM API、色彩空间改造或新的 GPU backend。代码修复作用于共享路径，实际 GPU 证据分别记录平台。
