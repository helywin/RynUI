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
- Core（含 Input/Text/动画）不得 include 或直接/间接链接 renderer；`renderer/common` 可以依赖 Core 和共同 renderer 模块。两者都不 include 具体 backend/OS SDK，也不得传递链接平台宿主、系统字体或 SDL。configure 的 include/link 守卫与实际 HEADLESS 构建负责验证；新增 backend 必须维持这一边界。
- 新组件只更新 logical CPU scene v2/stores，通过 SceneResources 上传事务与 SceneBackend 呈现；Quad/Glyph/RoundedEffect 的 GPU ABI、打包、shader reference 和 GPU 上传均属于 renderer，不得添加组件私有 SDL 上传路径。遵守 `docs/renderer-contract.md` 的 logical 单位、renderer/common packed GPU ABI v1、统一 SceneDeviceMetrics、owner/epoch、失败重试与析构顺序。
- Glyph atlas 上传只传 R8 source bytes/offset/row stride/目标 rectangle；transfer row/offset 对齐、padding 与 source ownership 由 backend 完成。backend 显式声明必需能力、ABI 与输入上限；共同资源 preflight 拒绝不兼容/超限，组件不得分支选择 backend 或静默丢弃基础视觉。
- 新 dirty 更新通过 FrameRequestState/DirtyQueues 唤醒；动画本帧 invalidation 与显式下一帧请求保持独立。callback host 使用非阻塞 tick、单调时间和 lifetime/generation ticket；deferred 不无条件立即重试。平台通用验证包含 `windows-msvc-headless` 的 Debug/Release CTest。
- 正式构建统一通过 `CMakePresets.json` 驱动并使用 `Ninja Multi-Config`；Windows 必须使用 MSVC，不得用 MinGW 结果代替 Windows 验收。
- 第三方依赖只允许显式 `BUNDLED|SYSTEM` 模式；版本、source SHA256 和 license 必须集中锁定，不使用 Git submodule 或隐式 system-first fallback。

## 代码格式

- 自有 C++ 与 HLSL 使用根目录 `.clang-format`，通过 clang-format 22.x 格式化，行宽 120、四空格缩进。
- `if`、`else`、`for`、`while`、`do` 的语句体必须使用大括号；`else if` 保留正常链式写法。
- 每条变量声明只声明一个变量，包含局部变量、成员和全局变量；不拆分结构化绑定、模板参数或函数参数。
- 相邻的 struct、class、enum 和函数定义之间保留一个空行，不把多个语句压在同一行。
- 完成 C++ 或 HLSL 修改后运行 `python scripts/format-code.py --check`；格式化和工具路径说明见 `docs/development/formatting.md`。第三方、fixture 和生成资产不参与批量格式化。

## OpenSpec 工作流

- 执行 OpenSpec 工作前，先读取当前操作对应的 `.agents/skills/openspec-*/SKILL.md`。
- 使用 pnpm 全局安装的 OpenSpec；CLI 与仓库生成 skills 当前配套版本为 `1.14.0`，安装及升级见 `docs/development/openspec.md`，不得在仓库安装 OpenSpec 依赖。
- change 名称使用 `NNN-YYYYMMDD-lowercase-kebab-case`，例如 `001-20260908-my-first-change`。
- `NNN` 是三位递增序号，`YYYYMMDD` 是创建日期，slug 必须表达具体目标。
- OpenSpec 说明性正文使用简体中文。
- `ADDED`、`MODIFIED`、`REMOVED`、`RENAMED`、`Requirement`、`Scenario`、`WHEN`、`THEN`、`SHALL`、`MUST` 等结构关键字保持英文。
- proposal 阶段只创建规划产物；没有明确进入 apply workflow 时不得实现代码。
- 不得把 planning complete、source reviewed 或 test designed 描述为功能已实现。

## 实施与提交

- 按 `tasks.md` 的依赖顺序实施。
- 每完成一个可独立验证的小阶段就创建一次 Git commit。
- Git commit message 必须使用英文 Conventional Commits，格式为 `type: description` 或 `type(scope): description`，例如 `refactor: isolate effect packing from Core`；不得只写无前缀的描述。
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

## 分平台验收清单

- 平台无关的实现、unit/headless test、contract test、benchmark 和文档校验只需在任一受支持平台完成一次；`tasks.md` 必须把它们列为平台通用任务并记录实际使用的平台与 preset，不得要求 Windows 与 Linux 重复验收同一逻辑合同。
- 只有依赖 OS、toolchain/ABI、window system、GPU/driver/shader、system font、input/DPI 或 packaging 的行为才建立分平台验收。此类 `tasks.md` 必须把 Windows 与 Linux 分成独立二级标题与独立 checkbox，不得在同一个 checkbox 中同时要求两个平台完成。
- 完整 CTest 若只验证平台通用逻辑，只需在一个平台运行；包含平台分支或平台集成的测试必须在受影响的平台分别运行，且不得用平台通用测试结果代替真实窗口、GPU 或系统服务证据。
- 每个平台的 checkbox 只有在对应操作系统的实际机器上完成后才能勾选。切换开发电脑时，已经完成的平台项保持完成，另一平台缺少证据不得使其回退，也不得由其代替。
- 存在分平台验收时，Windows 与 Linux 的证据和验收提交必须可以各自独立完成；最终收口只汇总本 change 明确要求的平台结果，不重复执行已经通过的平台通用验收。
