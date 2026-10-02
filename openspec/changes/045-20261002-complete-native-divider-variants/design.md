# Design

## Context

动机见 proposal。现有 Divider 为保留文字 wrapper 加可变 Quad content range；geometry measure 计算两侧 rail，color-only subscription 更新 Material；没有命中或 IME。034 public API 和测试覆盖 Theme/None/Ratio，但缺 variant、size、Start/End/direction 与 Length。共同 RetainedSurfaceService 已支持 content effects，RoundedEffect 的零 blur 圆角实心 shape 可用于圆点，无需 GPU ABI 改动。

## Goals / Non-Goals

**Goals:** 把新合同并入既有 retained Divider，保留旧 API、默认边距和 phased invalidation；资源有界、颜色与 geometry identity 分开；可在实际 Windows native preset 验证。

**Non-Goals:** 全局文字 bidi、DOM/CSS/Web 别名、交互 refs；方向只控制 Divider 标题/rail 位置，不声称改变文字排版。

## Decisions

1. 新增 `DividerVariant` Prop，默认 Solid；有效值为 Dotted 优先，然后 legacy dashed 或 Dashed，最后 Solid，与上游两种 class 共存的视觉优先级一致。可选 ControlSize Prop 只覆盖水平 margin：Small/Middle 对应 Theme small/middle margin，Large/缺省使用既有有文字/无文字 margin。新增两个 Divider metric/override，并纳入 inheritance、hash、诊断和 geometry identity；直接读取 alias margin 会使订阅漏失效，因此不采用。
2. 扩展现有 `DividerOrientation` 的 Start/End，新增 scoped `DividerDirection` LTR/RTL Prop。Left/Right 保持旧物理含义；逻辑方向在 measure 中归约为物理侧，不把 CSS 字符串或方向状态泄漏到 renderer。全局 Theme direction 属于后续 Theme/文字收尾，当前不引入跨组件未实现承诺。
3. `DividerOrientationMargin` 增加 Length source 与 `length(dp(...))` 构造；保留现有字段顺序和 ratio 规则。长度模式近侧 rail 为零、近侧标题 padding 为零、远侧保留 Theme text padding，长度作为近侧标题外间距；父约束不足时夹紧长度/远侧 padding/label 测量预算。None 保持现有两侧 padding 都零的兼容语义。全部输入在挂载前/更新时验证。
4. Solid/Dashed 保留 Quad 路径；Dotted 输出零 blur RoundedEffect 实心圆，每个 diameter 为 line_width、间隙为相同宽度。末端保留完整圆形但以 rail/window clip 裁剪；应用 node translation/opacity。每次先计算有限段数，组件总计不得超过 4096；整数索引循环避免小浮点步长不前进。range 继续保留，切换变体清空另一类 primitive，销毁走现有 owner cleanup。
5. 共同逻辑使用 Windows MSVC `windows-msvc-headless` Debug/Release Ninja Multi-Config CTest（补 portable Divider target）；新值、旧回归、彩色材质、圆点 reference coverage、裁剪/移动/资源清理与公开编译都验证。整合后完整 headless、configure guard、golden 增量校验。Windows native `windows-msvc` Debug/Release build/受影响 CTest 加真实 D3D12/DXIL、系统字体、三主题和系统/1/1.25/1.5/2 scale/resize。Linux GCC/Clang Vulkan/SPIR-V/Fontconfig/Wayland 必须实际 Linux 机器，独立 checkbox 保持待验证。

## Risks / Trade-offs

- [小线宽造成资源膨胀] → 上传前明确拒绝超过 4096 的请求，避免静默裁掉样式。
- [旧默认/Ratio/None 回归] → 既有 tests 不删除，新增 Length/size 覆盖不得改旧断言。
- [圆点颜色变更误测量/重建] → 分离 Geometry 与 Material，比较 node/scene/shape counters；变体改变 primitive 类型允许必要绘制拓扑变更。
- [方向范围被误解] → 公共文档和目录明确 scoped Divider direction，未声称全局 RTL/文字 bidi 已完成。

## Migration Plan

规划校验并提交后立即 apply；先实现 public/Theme/runtime/合同并独立提交，再整合目录/文档/完整共同验证，最后 Windows native 证据独立提交。无新增依赖；必要时回退阶段提交，旧声明仍可编译。Linux 证据后续独立完成，不 archive 或 push。
