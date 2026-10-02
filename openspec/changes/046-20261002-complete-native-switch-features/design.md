# Design

## Context

见 proposal 的动机。SelectionComponentHost 共用 Switch/Checkbox/Radio 的 press/focus 与 retained surface；Switch 当前 LeafLayout 固定宽度、两个 scalar target（handle/spinner），loading 合同明确保持焦点。Text/Icon 共用 glyph scene，已有 branch_active 支持只改变绘制遍历；ComponentLayout 支持持久子节点测量与放置。

2026-10-02 核对锁定来源：[Switch API](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/switch/index.tsx)、[Switch style/token](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/switch/style/index.ts)。原生采用内容与反馈设计合同，不搬运 DOM/CSS/React 语法。

## Goals / Non-Goals

Goals：保持旧调用、无内容尺寸、011 的 loading 焦点和 Checkbox/Radio 合同；state、颜色与动画更新不执行 slot。内容文字/图标走既有 semantic text style、shaping 和 glyph atlas。

Non-Goals：嵌套 Button/Input 等交互子控件、Web 的 value/defaultValue 别名、HTML 属性。Switch scoped direction 只镜像该控件，不引入全局 bidi 决策。

## Decisions

1. `SwitchCheckedContent`、`SwitchUncheckedContent` 与 `SwitchSlots` 包装两个 retained content 分支。布局同时测量两者，自然宽度取 max(content widths)+innerMinMargin+innerMaxMargin 与 trackMinWidth 的较大者；固定轨道高度。放置对应分支到手柄之外，按压时作小幅内容偏移。文本/图标内部 clip 使用节点继承的 logical rectangle 与窗口 clip 交集；branch_active 仅控制绘制，不影响宽度测量。相比销毁重建内容，保证 Signal 更新、字体缓存与身份连续。被动内容中出现交互节点时在 mount 事务内报错回滚。
2. ref 共享 owner-thread 状态与借用闭包，mount 前预留 binding；cleanup 撤销。onChange/onClick 在激活前复制，完成内部状态后依序调用，不在回调后访问状态；autoFocus 在资源与挂载列表完整后执行一次。保留 loading 的可聚焦但不可激活行为，避免破坏 011 明确合同。
3. Switch token 扩展四个 inner margins、handleShadow 和 wave 参数。尺寸/内容 typography/颜色/阴影通过 Theme identity 与派生值分别更新；opacityLoading 使用正式 Alias Token（锁定 alias.ts 的默认值为 0.65），Switch seed override 通过组件算法解析其 Map metrics。共同 surface 支持独立 shadow shape 与 fill 内部的 shadow 插入位置，避免重复绘制手柄或让阴影覆盖手柄。手柄伸展为 30%，受轨道可用空间限制，RTL 镜像伸展方向；shadow 使用 RoundedEffect，不改 renderer ABI。
4. wave 使用第三个 scalar target、lazy component-owned effect range 和有限 slow/easeOut transition，与 Button 同一 logical outline/effect/AnimationEngine 基础；不共享 Button Component Token。重复激活重启，完成清空 effect 并保留可复用 range，停止条件覆盖所有取消路径。handle/spinner 既有 target 保持；非 loading 稳态无请求下一帧。
5. 平台通用通过 Windows `windows-msvc-headless` Debug/Release 完整 CTest（含 Core 边界 guards），额外 native Gallery/字体合同测试；原生专属通过 `windows-msvc` Debug/Release 实窗 D3D12/DXIL/GPU readback，系统 DPI 与四倍率，独立 Linux checkbox。本机没有 Linux 原生验收证据。

## Risks / Trade-offs

- [长内容/窄约束] → 子布局使用内容预算、glyph 继承 clip；覆盖长 CJK、零/窄宽度、动态内容和 RTL。
- [shared ref 冲突、mount 抛错] → 预留与 cleanup、重复绑定/跨线程/重绑定/嵌套交互回滚测试。
- [色彩造成无关测量] → 仅派生 metrics 变化调用 Measure；内容 foreground 更新保持 glyph shaping 不变。
- [动画与 callback 销毁] → 回调副本、animation scope 释放、lazy range 清理、取消/重启/idle 测试。
- [主题 JSON 和 Gallery 数量基线变化] → 只增量更新新字段、相关样例和 goldens，验证旧字段不变。

## Migration Plan

增量新增 API，不修改旧调用；每个独立阶段验收后提交。出现回归时回退该阶段提交。不会 push、创建 PR 或 archive。
