# Design

## Context

动机见 proposal。ComponentHost 现有 before/after children paint traversal；窗口共同服务管理 component、focus、pointer、text 与 surface。TextHost 先布局再同步文字，浮层必须插入两者之间，保证同帧 placement。Gallery 目前 11 个 partial 条目，Radio 实现漏标；固定数量测试仍使用 10/62。

## Goals / Non-Goals

Goals：完整交付原生基础文字 Tooltip，并建立不污染 renderer 的可复用窗口层。原生组件收尾清单记录实际缺口，后续按依赖实施。

Non-Goals：见 proposal。Web props 不作为原生组件未完成理由；系统 accessibility bridge 属于平台能力，不以本 change 宣称已交付。

## Decisions

1. Tooltip 使用 wrapper、trigger slot 和 persistent popup 子组件。wrapper ComponentLayout 只测量 trigger；popup 在窗口 after-layout 阶段单独 measure/place。保留父 component/scope/theme lifetime，popup 不进入普通布局尺寸。
2. ComponentHost 提供内部窗口浮层 root 标记与稳定 priority；普通遍历跳过这些子树，再按 priority/declaration order 输出浮层。branch_active 同时控制 paint 和文本。相比只把 body fragment 挪到末尾，此方案保证 body、arrow、text 整体顺序。
3. InteractionRegistry 为未指定 parent 的 registration 解析最近 component ancestor interaction，统一 Tooltip wrapper 与现有 Button/Input/selection 的传播。focus 用共同服务在帧开始/布局同步前观察；Escape 通过 FocusManager 的窗口预处理器分派给 participant，不覆写 child handlers。
4. hover/focus 用整数 microsecond 单调时间和 generation ComponentId 保存 deadline；WindowComponentServices 的辅助 deadline 参与已有帧调度。状态变更前复制 callback，callback 后重新获取 state，避免重入销毁引用。Escape dismissal latch 在离开所有 trigger 后重置。
5. positioning 使用纯 CPU logical geometry，十二种位置，main-axis flip 比较溢出，再窗口 clamp。窗口边距和 popup maxWidth 控制测量，箭头使用共同 logical quad strips，不新增 GPU ABI；body 的 shadow 由 retained effects 消费。
6. Tooltip typed token 分颜色、metrics、typography/order；derive 来自锁定 Ant Design 6.6.5 [style](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/style/index.ts) 与 [shared API](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/shared/sharedProps.en-US.md)，maxWidth=250、zIndexPopupBase+70、padding 6/8、controlHeight 与 Theme radius。继承/algorithm/JSON/hash 与现有 Slider 模式一致。

## Risks / Trade-offs

- 公共 paint/input 行为影响旧组件 → 保留显式 parent，补树层/传播/隐藏/重入测试，完整 headless/native CTest。
- 极小 viewport 与长文本 → 限制可用宽高，finite validation，文本与箭头 clip 到窗口。
- 计时和 focus 变化 → 可控时钟验收，无 deadline 时 idle 保持静止。
- 本机没有实际 Linux native desktop → Linux 专属项独立保留，不能用 Windows/WSL 代替。

## Migration Plan

规划通过 doctor/full strict/diff 后提交，按用户明确授权立即 apply。先提交已验证基础层，再提交 Tooltip/API/Theme/Gallery 整体，再 Windows evidence；使用 CMakePresets/Ninja Multi-Config，Windows MSVC headless Debug/Release 为平台通用合同，native Debug/Release 验证 GPU/字体/输入/scale。Linux 使用其实际 GCC/Clang native preset 独立验收。无需改依赖或 GPU ABI，回退本 change 提交可恢复旧行为。
