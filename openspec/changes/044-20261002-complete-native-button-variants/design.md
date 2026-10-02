# Design

## Context

见 proposal.md。`button.hpp` 只有四种类型；`button_component.cpp` 使用 HorizontalContentLayout、十个固定 quad 和五个 MaterialTransition 通道；shared retained content ranges 已支持变量数量的 RoundedEffect，提供波纹与透明虚线所需的共同路径。Theme 使用显式 identity 分组；新增颜色必须同步派生、override、序列化/hash、订阅变更比较。原生 host 已支持 Tooltip 的 post-route secondary input。

## Goals / Non-Goals

**Goals:** 公共选择器与 retained slots；普通颜色更新仍只 Material，位置/尺寸更新只影响本组件；真实透明 gap 和圆角、不改 packed GPU ABI；非阻塞有限 deadline；旧 callers 编译/行为兼容。

**Non-Goals:** 见 proposal；双汉字自动空格是上游 React/DOM textContent 特例，原生 slot 保留明确 Text 排版。Space.Compact 后续处理共享边界，不在此复制外部样式。

## Decisions

1. 独立 `button_types.hpp` 保存颜色/变体/形状/位置 enum，Button 和 Theme 共同引用。每个预设色的 base/hover/active/light 等由 theme 模块使用既有 Ant palette 算法派生并缓存在 Button token；主色/危险色尊重 component seed 和 override。新增 colors/metrics 进入原有对应 identity，所有新数据参与 hash/serialization。颜色不在 Button 内硬编码。未配置新选择器时保留旧类型路径（包括旧 Text hover token），配置后按 spec 的原生独立优先级组合，而不复刻上游只接受 color+variant 成对的 React 逻辑。
2. 保留旧 Button(props, content)；新增 typed icon/loading slots 与 icon-only 入口。wrapper 存在时使用一次挂载的常规/加载子树，由 ComponentLayout 选择当前子树测量/放置并由 branch_active 控制绘制。HorizontalContentLayout 增加末端排布和跳过首个图标 wrapper 的字段，默认值保留旧规则；builtin spinner 为隐式 item，custom loading 为 wrapper item，避免同时计数。shape/block/icon-only 在控件布局处理，外部 width 优先。
3. 虚线使用共同 RoundedEffect outline 的圆角边缘与各段 clip，在 retained content effects 发布；水平/垂直带不重叠，ghost 的 gap 保持真实透明。舍弃用背景色覆盖实线的方法，因为它破坏透明背景；舍弃新增 dash GPU 字段，因为本 change 不修改 ABI。内容范围上限 4096 明确拒绝超大/极小 dash 组合，不静默丢弃。按几何缓存 outline clips，动画/颜色只更新已有几何的 material。
4. Wave 是一个额外 outline effect：主题的 spread/opacity 与 motion duration/easing 控制扩散淡出；独立 scalar 动画通道，复用 generation-safe AnimationRuntime，activation 前设置反馈、复制 callback 后允许销毁，销毁无需读取失效 state。关闭/禁用/加载/无边框/窗口失活/减少动画立即取消。影子/focus 的现有 surface 独立保留。
5. loading(bool) 保留；新增 loadingDelay(Duration) 和 typed loading slot。截止时间纳入 host deadline/tick，在 owner thread 处理，不使用睡眠/平台定时器；取消时清掉请求。ButtonRef 按 SliderRef 的 shared binding、owner thread、组件代际与 cleanup 合同实现。autoFocus 只读初始值，布局前统一 focus 管理，不劫持 disabled 焦点。
6. Windows MSVC / Ninja Multi-Config 的 `windows-msvc-headless` Debug/Release 验证共同合同；`windows-msvc` Debug/Release 完整 build 和 affected CTest，真实 SDL D3D12/DXIL 默认/暗/紧凑、系统字体、原生 pointer/keyboard 与 DPI 矩阵。Linux GCC/Clang、Vulkan/SPIR-V、Fontconfig/Wayland 独立 pending；当前 Windows 不能替代。

## Risks / Trade-offs

- [新增 tokens 未进入 identity 导致漏更新] → theme/runtime/hash/override 单测及颜色不测量断言。
- [虚线角部 clip 接缝和高 DPI] → 圆角 outline 统一 AA、分带无重叠；实际透明背景/GPU 图像验收。
- [图标替换改变测量、loading delay 泄漏] → wrapper retention、取消/反复切换、idle deadline 和销毁资源计数。
- [wave 影响现有静止断言] → 有限通道与明确 motion policy；旧动画测试按行为添加终止检查，不能通过关闭产品默认 wave 掩盖。
- [长边框超出共同 effect 限制] → 计算前检查需求数量，抛出明确异常；不为通过测试静默截断。

## Migration Plan

先规划并校验提交；依赖顺序为 token/variant/dashed、slot/layout/focus/delay、wave、Gallery/common regression、Windows。每阶段可独立验证并提交；旧 API 保留，无破坏性迁移。Linux evidence 单独提交；不自动 push/archive。

## Sources

2026-10-02 查阅锁定 Ant Design 6.6.5 的 [Button](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/Button.tsx)、[buttonHelpers](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/buttonHelpers.tsx)、[variant](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/style/variant.ts)、[token](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/style/token.ts)、[styles](https://github.com/ant-design/ant-design/blob/6.6.5/components/button/style/index.ts)、[WaveEffect](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/WaveEffect.tsx)。仅使用原生可观察合同，不复制 React/CSS 实现；palette 派生复用仓库锁定的 @ant-design/colors 8.0.1 算法。
