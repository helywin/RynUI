# Design

## Context

基线为本地 34eefb82，含 Avatar，不读取未集成 Rate。Ant Design 6.6.5 参考位于 D:/code/RynUI/out/upstream-ant-design-6.6.5，HEAD 4a39f54842eade4e565ab336ef6097cd7e723cdd；参考 components/tree 文档、Tree.tsx、style、basic/basic-controlled/customized-icon demo 和测试 snapshots。现有 Steps/Menu 提供 typed slot、roving 和 interaction 模式，Collapse 提供保留分支，Scroll/Focus 与共享窗口输入边界承担真实输入清理。

## Goals / Non-Goals

**Goals:** 保留公开 typed API 与固定内容身份；模型独立可验证；组件只写 logical scene；有限布局允许非虚拟滚动并显露焦点。

**Non-Goals:** 见 proposal 的首版排除项；不改变 renderer/GPU ABI、Core 分层、宿主 SDK、Gallery 或依赖锁。

## Decisions

- 将拓扑按前序展平，保存 parent/depth/subtree end；限制 1024 节点、64 层并在挂载前拒绝超限。每次状态更新线性计算可见行及勾选，避免动态树差分和虚拟化复杂性。
- selectedKeys 仅单选；勾选为非 strict conduction。禁用节点阻断向下传播与向上汇总，但不作为 wrapper 的全子树禁用，保证有效富 child 与独立子节点可用。
- 每行保留 switcher、checkbox 和 label/icon 区域；行只有一个原生 roving interaction；checkbox 由 Tree 自绘并通过同一 interaction 分区操作，避免额外 Tab。富 slot 原生 interaction 优先，不穿透最近 ownership。
- 使用独立组件包围私有分区；所有用户调用边界后重取完整 ComponentId 及私有结构，缺失时仅退役旧 owner。挂载中先连接状态再渲染最终状态，不跳过首次订阅值。
- 隐藏行以 branch_active/out_of_flow 移出布局，并对整行设置输入 block 以关闭无焦点 popup。禁用仅禁用行操作；富内容保持自己可用状态。每个 owned block 在 cleanup 注销。
- 根布局维护有限滚动 offset；行的固定最小高度和富内容自然高度共同决定高度。内容/填充/命中按行和分区裁剪，焦点装饰经 external clip；用实际 effect/glyph/hit 记录验收。
- 新 Tree Theme 的两个 TokenIdentity、snapshot 片段和解析 IIFE 只尾追加；不改旧 typed 值、旧 IIFE 顺序。所有 ThemeConfig 大对象放 heap，PE 保持默认 1MiB 栈。

## Risks / Trade-offs

- 富 slot/Prop/Theme 回调可销毁任意私有节点 → 每次跨调用边界核验完整结构和代际，保存值而非跨回调引用。
- 非虚拟树线性测量成本 → 明确有界规模，不静默截断，不引入多余调度或动画 deadline。
- popup/session 可能不持焦点 → 使用共享输入边界并测试 Select 的隐藏恢复。
- 本机只能证明 HEADLESS logical scene → MSVC windows-msvc-headless、Ninja Multi-Config，Debug/Release 最多 2 jobs；不声称原生 GPU/Linux/系统 IME/AT 通过。

## Migration Plan

新增 API，无已有调用迁移；分规划、模型/Theme、组件/验证本地提交。父线程独立审查与集成，不访问远端，不 archive。
