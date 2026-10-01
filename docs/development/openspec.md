# OpenSpec 开发工具

RynUI 使用全局安装的 OpenSpec CLI。当前仓库 agent skills 由 **OpenSpec 1.14.0** 生成，配套 CLI 使用相同版本。OpenSpec 仅用于开发工作流，不进入 C++ 应用运行时，也不在仓库安装 Node.js 依赖。

## 安装与升级

准备 Node.js 20.19.0 或更高版本及 pnpm，然后执行：

```powershell
pnpm add --global @fission-ai/openspec@1.14.0
openspec --version
```

切换到 RynUI 仓库根目录后，刷新已配置的 agent skills：

```powershell
openspec update
```

`openspec update` 更新 `.agents/skills/openspec-*/SKILL.md` 等生成指引，不替代 CLI 包升级。更新后检查 Git diff，保留仓库的 `AGENTS.md`、`openspec/config.yaml` 与现有 change 约定。升级到其他版本时，应先确认上游发布版本，再更新全局 CLI、生成指引与本文的配套版本。

上游要求与更新流程见 [OpenSpec 官方说明](https://github.com/Fission-AI/OpenSpec#updating-openspec)。仓库使用带明确版本的 pnpm 安装命令，以避免不同机器静默跟随 `latest`。

## 校验

规划和文档变更至少运行：

```powershell
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

`doctor` 检查 OpenSpec 根目录与引用健康；strict validate 检查 spec/change 结构。两者均不证明 C++ 功能、GPU、真实窗口或设备验收通过。

若 `openspec --version` 仍返回旧版，用 `Get-Command openspec`（Windows）或 `command -v openspec`（Linux/macOS）确认 PATH 实际解析的全局入口。旧的 1.4.1 CLI 不支持仓库要求的 `doctor`；应修复全局版本或 PATH，不跳过校验。
