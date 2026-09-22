# Design

## Context

见 [proposal.md](proposal.md)。官方 [Ant Design 6.6.5 release](https://github.com/ant-design/ant-design/releases/tag/6.6.5) 在 2026-09-22 被核实为最新正式版。仓库目前从 `gallery/ant-design/6.5.0/` 和 `design-tokens/ant-design/6.5.0/` 离线生成目录与 1194 条 Token metadata；生成工具、Theme、Button/Input source contract、Gallery evidence verifier 和当前文档还直接引用旧版本。change 008 留有未完成的 Windows/Linux 验收，011 的规划也引用 6.5.0。历史结果不能冒充新版验证。

## Goals / Non-Goals

**Goals:**

- 将 6.6.5 作为全项目唯一当前设计参考，实际比对上游版本差异后更新生效 manifest、Token、组件状态、Gallery、测试和当前文档。
- 让这次“最新”选择可复现：记录完整 release commit、逐文件 SHA256 与 license；构建/运行不追浮动网络版本。
- 保留 6.5.0 的历史输入和证据作为可审计快照，重新留存 6.6.5 的自动和真实平台证据。

**Non-Goals:**

- 不把 Ant Design React/CSS-in-JS 或其运行时加入 RynUI，不重做不受差异影响的 native 组件。
- 不自动完成 change 008 的旧 checkbox，不修改旧截图/退出码；不在本 change 实现 011 的 Switch/Checkbox。
- 不让未来版本自动进入构建；后续“最新”需要新的显式评估与升级。

## Decisions

### 1. 版本化并列保存，只有一个当前入口

保留 `6.5.0` 目录作为历史快照，新建 `6.6.5` 的 Gallery source manifest、support overlay、Token catalog、source lock 与 golden。生成工具的当前版本常量和路径一次切到 6.6.5，并增加跨生成物 identity 检查；`--check` 在无网络、无 Python cache 的条件下复现。不要直接覆盖 6.5.0 文件，否则旧 evidence 中的 hash 和来源将失去可追溯性。旧版专项测试可作为历史回归保留，但不能同时宣称两版是当前基线。

备选是原地改写旧目录，文件数较少却破坏历史可审计性；另一备选是构建时查询 `latest`，会导致同一 commit 的二进制和测试结果随时间变化，因此不采用。

### 2. 先形成差异账本，再迁移运行值

以 6.5.0 与 6.6.5 的官方 tag/commit 对照 `docs/spec`、`components/overview`、目录源、Token 源以及 Button/Input/Switch/Checkbox 的 API/style 源。差异账本逐项记录新增、移除、改名、默认值/派生、状态视觉和 API 变化，并附 source path、commit 与 SHA。分类条目数、Token 条目数及 hash 由新版实际数据计算，不预设仍为 72/1194。若来源无差异，记录来源等价后保留既有逻辑，避免人为数值漂移。对于无法以现有 RynUI 类型表达的新 Token，明确 catalog-only 或新增 typed mapping；不得默默忽略。

备选是仅把 `6.5.0` 全局替换成 `6.6.5`，会让旧值伪装为新来源，且无法解释测试回归，故先做差异再改生成器和 runtime。

### 3. 保持公开 API 稳定并显式处理兼容性

Button、Input、Theme 的现有 typed C++ API 不因上游 React Prop 改名而机械变化。仅当 6.6.5 的可观察设计合同确有变化，才更新 Component Token 映射、默认 Theme、scene 状态或公开语义；每项记录旧/新对照与 consumer 兼容性。`LayoutStyle` 仍只管外部布局，视觉只由 Theme/Component Token 提供。011 在该基线实际落地后再经 OpenSpec update workflow 修订到 6.6.5，并纠正 Switch/Checkbox 的尺寸与 indeterminate 描述；不得在 012 中直接实现新组件。

### 4. 证据分层而不挪用历史通过项

新增 6.6.5 的平台通用证据合同，包含 release identity、目录/Token counts/hash、差异账本、Button/Input 回归、generator `--check`、依赖/许可、benchmark 与实际 preset。Windows 与 Linux 仅为发生实际平台差异的行为各建独立 evidence 和 checkbox，真实窗口需记录系统 scale、字体、GPU/shader、截图与退出码。旧 change 的 proposal/spec/tasks/evidence 保留其 6.5.0 当时事实；必要时在当前文档中标明已被 012 新基线取代，但不在旧记录里倒填通过状态。

### 5. 构建与迁移顺序

正式构建仍用 `CMakePresets.json` 与 `Ninja Multi-Config`。先在一个受支持平台完成离线来源与平台通用合同；Windows 使用 MSVC x64 完成受影响的 Win32/D3D12/DXIL 构建与实窗，Linux 在原生 Wayland 使用 GCC/Clang 与 Vulkan/SPIR-V 另行验收。平台通用测试不要求两边重复，平台特有分支不得用另一平台替代。当前主机只能直接完成 Windows；Linux 保留独立待验收状态，最终收口在两平台证据齐全之后。

## Risks / Trade-offs

- [新版 Token/目录差异改变既有外观] → 保留旧/新来源与视觉对照，先修改受影响映射，再运行 Button/Input/Gallery 相关回归和实窗复验。
- [大量生成物造成无关改动] → 由离线工具生成，先审查差异摘要、source lock 和 `--check`，只提交当前阶段文件。
- [旧 6.5.0 合同与新 6.6.5 合同同时被当前测试读取] → 区分历史测试和当前 evidence gate；当前 gate 拒绝交叉版本。
- [在途 008/011 与新基线不一致] → 008 保留历史状态，011 在新版来源确认后通过独立规划更新对齐，再恢复实施。
- [上游又发布新版本] → 本 change 使用创建时核实的 6.6.5；不在实施中无审查地追涨版本，必要时另建升级 change。

## Migration Plan

依次完成：核实完整 release commit 与差异账本；建立 6.6.5 离线输入；迁移 Token/Theme 与组件；迁移 Gallery/文档；平台通用验收；独立 Windows/Linux 真实验收；最后协调 011 规划并收口。每个可独立验证阶段分别提交。若某阶段不能通过，保留既有 6.5.0 当前运行基线及历史数据，不把半迁移输出标成通过；在新当前入口切换前确保所有生成器与测试指向同一版本。
