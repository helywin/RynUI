# Design

## Context

动机见 proposal.md。已阅读 Tooltip 的 position_tooltip/箭头字形/延时、Popconfirm 的 retained slots、Dropdown 的 native activation gate 和 wheel fallback，以及 Date/Time 的最近 layer 与回调回取路径。WindowComponentServices 按 host 注册顺序定位、逆序派发键盘和 wheel，因此 Popover 必须在 Date/Time/Dropdown/Tooltip/Select 前挂接，让子层优先；父层位置先提交再更新内容 Table 与子定位。

锁定参考位于本机 `D:/code/RynUI/out/upstream-ant-design-6.6.5`，HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`；仅只读核对。Popover 采用其中 elevated 背景、heading/title、strong weight、titleMinWidth177、padding12、title gap8、radiusLG、z-index base+30 的普通模式语义。

## Goals / Non-Goals

**Goals:** typed reactive API、富内容身份保留、原生激活和输入归属、可验证的有限视口行为、完整独立 Theme。

**Non-Goals:** 不创建通用 overlay 框架；不修改 logical scene v3/GPU ABI、Core 边界、已有 Theme IIFE/identity 顺序、Time 静态配置或 Binding bridge；不扩大 PE 栈，不新增依赖。不更改 Gallery、support-overlay、README 或全局完成表。

## Decisions

- 复用 position_tooltip 与箭头 glyph；独立小型 Popover model 管理受控请求去重、边界 suppression 和触发原因/延时。不会将富内容作为 Tooltip title，因为真实子 Button 必须得到 pointer 和 keyboard 激活。
- 首次创建 trigger、popup、title、content、arrow 的持久组件。popup 是非模态 window layer，关闭只切换 branch；文字走 Signal，typed slots 仅挂载一次。提供 hover/focus/click 三布尔组合，全部 false 表示 manual，默认 hover。拒绝枚举/配置冲突和超过一天的延时。
- 普通关闭尊重 controlled authority；强制 suppression 无条件关闭有效视图、取消 deadline、pointer 和子输入。使用本组件 popup root 的 owned block，并在销毁时只移除自身 block；对子控件触发已存在的 boundary 生命周期。
- hover 从命中逻辑 ancestry 判断，包含子 window layers；focus 使用完整逻辑 ancestry。点击打开不强抢富内容焦点，键盘打开将焦点延迟移入首个可焦点内容；关闭只恢复实际在内容内的原焦点，外部点击不抢焦点。无焦点 defaultOpen 仍参与 boundary 检查。
- 自身处理前给编辑器与子 popup 优先，内容 wheel 处理后仅最近 popup 边界 fallback。原生 trigger activation gate 和所有用户可执行路径之后重新查询完整 ComponentId generation。
- 内容按视口减 padding 测量，title 可见时应用 titleMinWidth；超高内容裁剪，不提供内部滚动。popup content clip 约束普通内容；focus decoration 的外扩只受有效外部 clip 约束，子 popup 保持现有 window layer 逃逸规则。
- Theme 新增独立 token/config 与 snapshot/runtime 五组 identity，append-only；新增派生函数和末尾 resolve 路径，不重写原有33个 IIFE。新构建程序生成五份 goldens，再逐值比较所有旧字段（identity 除外），验证无旧值漂移。

## Risks / Trade-offs

- [同步 onBlur/onClick/getter 销毁] → 回调前保存 identities，之后重新回取并检查 active scope；实测卸载与重绑。
- [嵌套 host 定位顺序] → 父 Popover 先定位，同类型父先于子；对已有较早 host 做必要定向再定位，不引入新 overlay 抽象。
- [Theme 大对象和默认 1MiB 栈] → 新 component token 通过堆持有，保留旧 IIFE、heap descendant preflight 和 self guard；测试配置避免大型按值堆叠。
- [平台证据边界] → 只在本机 Windows 使用 MSVC Ninja Multi-Config `windows-msvc-headless` Debug/Release，最多4 jobs，针对本次合同去重测试。native/GPU/Linux/system IME/AT 不在本次 headless 证据内。

## Migration Plan

新增 API 无破坏性迁移。规划和实现分阶段英文 Conventional 本地提交，由父任务集成；不访问远端、不 push、不创建 PR 或 archive。回滚可以撤销本 change 的实现提交。
