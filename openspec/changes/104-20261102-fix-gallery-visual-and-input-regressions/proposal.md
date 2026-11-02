# Proposal

## Why

用户在最新版 Windows Release Gallery 中确认了描边不均、圆形切边、内容裁剪泄漏、文字与图标偏心、字体缺字和输入交互缺失等问题。多个控件共用同一缺陷路径，需要修复实际渲染和布局，不能通过样例偏移或字符替代掩盖。

## What Changes

- 统一 Quad 与 RoundedEffect 的物理 AA 支持域，修复 Table 后代填充裁剪和相邻选中边的合成顺序。
- 修复 Tooltip 接缝、圆角虚线周长节奏及 Dashboard 弧端，保留透明度、真实硬裁剪和设计开口。
- 核对并修复共享文字/图标度量及 Alert、Tree/TreeSelect、Avatar、OTP、InputNumber、Modal 的实际布局。
- 补全 Windows 系统字体中的常见 emoji fallback，以及文本输入区域的 I-beam 鼠标提示。
- 修复 Switch 内容/滑块动画及小尺寸 RTL，核对 Timeline loading 图标。
- 用针对性回归和 Windows Release 窗口检查覆盖本批 25 条反馈；原始产物仅留 `out/`。

## Capabilities

### New Capabilities

- `scene-shape-coverage`：形状的完整物理覆盖、描边合成、路径接缝与祖先裁剪。
- `text-control-geometry`：文字、图标、编辑光标和控件内容的测量与居中。
- `system-font-fallback`：系统字体链对示例 Unicode 内容的真实字形选择与光栅化。
- `native-pointer-cursor`：文本编辑区域的逻辑鼠标样式及平台宿主显示。
- `selection-control-transitions`：Switch 内容与手柄的连续动画及状态门禁。

### Modified Capabilities

无。当前 `openspec/specs` 尚无已同步的 capability；本次增加可持续的行为合同，不归档历史 change。

## Impact

涉及 graphics、renderer/common、HLSL/SDL 消费端、文字/布局及相关组件、Windows 字体与输入宿主和测试。保留 Core/renderer/平台边界、logical scene v3 与 packed ABI v2、typed API、retained 身份及既有性能优化；不引入 Qt/qrc、颜色 emoji 管线或新 backend。风险主要是 fractional DPI 下的覆盖、真实裁剪和输入/焦点生命周期。平台通用合同用 MSVC HEADLESS 验证一次；Windows 原生单列，Linux 真实 GPU/字体/鼠标验收不由本机替代。
