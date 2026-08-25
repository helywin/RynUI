# RynUI Agent 协作规则

## 适用范围

本文件适用于整个 RynUI 仓库，保存 coding agent 的执行规则。产品介绍属于 `README.md`，完整技术决策属于 `docs/architecture.md`，具体变更计划属于 `openspec/changes/`；不要在这些文件之间复制整段内容。

## 文档职责

- `README.md`：面向使用者和项目参与者，只保留项目定位、当前状态和文档入口。
- `AGENTS.md`：面向 coding agent，保存工作流、约束、验证和提交规则。
- `docs/architecture.md`：RynUI 的正式架构基线和长期技术决策。
- `openspec/config.yaml`：OpenSpec 的语言、命名与产物规则。
- `openspec/changes/<change>/`：单个变更的 proposal、spec、design、tasks 和验收范围。

发生冲突时，当前已批准的 OpenSpec change 决定本次工作范围，`docs/architecture.md` 决定长期架构边界；不得用 README 摘要覆盖详细设计。

## 项目约定

- 项目名称统一使用 `RynUI`。
- 公开 C++ API 使用 `ryn` 命名空间。
- 基础 UI 组件、公开布局、Design Token、主题和交互状态以 Ant Design 6 为设计基线。
- 公开组件采用 typed Props、typed slots 和 reactive `Prop<T>`。
- `LayoutStyle` 只控制外部布局；稳定组件的视觉样式只通过 Theme 与 Component Token 控制。
- Compose 只作为 slot composition、Constraints 和 phased invalidation 的机制参考，不使用通用 `Modifier` 作为稳定组件的视觉入口。
- 不使用 Virtual DOM；普通属性更新不得重新执行无关 Component。
- SDL3 类型不得泄漏到 Reactive、Layout、Component 或公开 API。
- 正式构建统一通过 `CMakePresets.json` 驱动并使用 `Ninja Multi-Config`；Windows 必须使用 MSVC，不得用 MinGW 结果代替 Windows 验收。
- 第三方依赖只允许显式 `BUNDLED|SYSTEM` 模式；版本、source SHA256 和 license 必须集中锁定，不使用 Git submodule 或隐式 system-first fallback。

## OpenSpec 工作流

- 执行 OpenSpec 工作前，先读取当前操作对应的 `.agents/skills/openspec-*/SKILL.md`。
- change 名称使用 `NNN-YYYYMMDD-lowercase-kebab-case`，例如 `001-20260908-my-first-change`。
- `NNN` 是三位递增序号，`YYYYMMDD` 是创建日期，slug 必须表达具体目标。
- OpenSpec 说明性正文使用简体中文。
- `ADDED`、`MODIFIED`、`REMOVED`、`RENAMED`、`Requirement`、`Scenario`、`WHEN`、`THEN`、`SHALL`、`MUST` 等结构关键字保持英文。
- proposal 阶段只创建规划产物；没有明确进入 apply workflow 时不得实现代码。
- 不得把 planning complete、source reviewed 或 test designed 描述为功能已实现。

## 实施与提交

- 按 `tasks.md` 的依赖顺序实施。
- 每完成一个可独立验证的小阶段就创建一次 Git commit。
- Git commit message 必须使用英文。
- 一个提交只包含当前阶段相关文件，不混入用户的其他改动。
- 提交前运行该阶段列出的测试和验收；未通过的任务不得勾选。
- 不主动 push、创建 PR 或 archive change，除非用户明确要求。

## 最低验证

规划或文档变更至少运行：

```text
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

代码阶段还必须运行 `tasks.md` 指定的 build、CTest、benchmark 或真实窗口验收。Windows、Linux 或 GPU 行为只有在对应环境实际运行后才能报告通过。
