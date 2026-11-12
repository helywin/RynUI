# OpenSpec 开发工具

RynUI 使用全局安装的 OpenSpec CLI。当前仓库 agent skills 由 **OpenSpec 2.0.0** 生成，配套 CLI 使用相同版本。OpenSpec 仅用于开发工作流，不进入 C++ 应用运行时，也不在仓库安装 Node.js 依赖。

## 安装与升级

准备 Node.js 22.12.0 或更高版本及 pnpm，然后执行：

```powershell
pnpm add --global @fission-ai/openspec@2.0.0
openspec --version
```

切换到 RynUI 仓库根目录后，刷新已配置的 agent skills：

```powershell
openspec update
```

更新前先运行 `openspec update --dry-run --json` 核对写入/删除范围，并保存自定义指引的副本。2.0.0 将生成技能统一到 `.agents/skills/openspec-*/SKILL.md`，并移除已生成的旧工具专属 slash command；workflow 本身不变。`openspec update` 会覆盖该目录下的生成指引，不能把项目约定只写在生成技能中。

`openspec update` 不替代 CLI 包升级。更新后检查 Git diff，保留仓库的 `AGENTS.md`、`openspec/config.yaml` 与现有 change 约定。升级到其他版本时，应先确认上游发布版本，再更新全局 CLI、生成指引与本文的配套版本。

上游要求与更新流程见 [OpenSpec 官方说明](https://github.com/Fission-AI/OpenSpec#updating-openspec)。仓库使用带明确版本的 pnpm 安装命令，以避免不同机器静默跟随 `latest`。

2026-11-12 按用户要求升级到官方 latest **2.0.0**，官方 [release](https://github.com/Fission-AI/OpenSpec/releases/tag/v2.0.0) 与 npm registry 已核验。历史 evidence 中的 1.14.0 / 1.14.3 / 1.17.0 属于当时实际工具版本，不追溯改写为 2.0.0。本次工具迁移不修改 change 任务完成状态。

## 校验

规划和文档变更至少运行：

```powershell
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

`doctor` 检查 OpenSpec 根目录与引用健康；strict validate 检查 spec/change 结构。两者均不证明 C++ 功能、GPU、真实窗口或设备验收通过。

## 人工检查与任务清单

自 2026-11-03 起，按用户要求，`tasks.md` 只追踪实现、自动化测试、自动平台集成和文档校验。不要添加人工操作、目视核对、用户确认、跨显示器手动检查或系统 IME 手动验收的 checkbox，也不要把这些要求混入自动任务。可选体验建议放在 change 的验收说明中，不阻塞实施或 archive。

历史未完成的人工项按用户指示勾选并标注“人工要求已关闭，非实测通过”；这表示取消该任务门槛，不表示执行了测试。混合条目拆开后，实现、构建、CTest、自动 GPU/窗口集成等未完成部分继续保持未勾选。已有实测记录保留，未覆盖的平台、驱动、IME 或显示环境仍在说明中据实列出，不因清单关闭而增加通过数量。

本次仅整理规则与历史清单，不运行新的人工或窗口验收，不自动 archive change。

## 验收结果的保存

执行测试后，只在 change 的 Markdown 中记录平台/preset、命令、通过/失败数量、结论及必要限制。原始日志、CSV/JSON/XML、截图/逐帧图、源码副本和产物索引留在已忽略的 `out/`，不进入 Git；具体约束见根目录 `AGENTS.md`。

2026-10-30 已清理历史 evidence 运行产物，保留文字结果。旧文档中的原始附件路径属于历史记录；本机归档位于 `out/evidence-archive-20261030/`。Git 历史未重写，需要追溯时仍可从旧提交取回。必要的确定性测试输入与 schema 已移至 `tests/fixtures/`，不以历史截图存在与否判定当前功能是否通过。

若 `openspec --version` 仍返回旧版，用 `Get-Command openspec`（Windows）或 `command -v openspec`（Linux/macOS）确认 PATH 实际解析的全局入口。旧的 1.4.1 CLI 不支持仓库要求的 `doctor`；应修复全局版本或 PATH，不跳过校验。

## 2.0.0 迁移校验

本仓库原已使用统一 `.agents/skills`，dry-run 仅列 6 个已有生成技能的更新，没有旧工具专属生成 command 待删；`openspec update` 已按该计划执行，并重读新的 apply 指引。实际 CLI 2.0.0：doctor healthy，strict **109 passed / 0 failed**，diff check 通过；全部 109 个 `tasks.md` 的 checkbox 身份/顺序/状态向量及 `openspec/config.yaml` 原样保留。C++/change artifacts 和进行中的实施差量未混入此工具提交，历史工具版本证据未改写。本阶段不运行 C++ 或 GUI 矩阵。
