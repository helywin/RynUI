# 已发布 main 580 共存验收

## 显式合并边界

R1 分区裁剪修复先独立验证并保存为干净 `ee2b9efde55f32b1a111165c9d44e48c9286a387`，详见 [R1 证据](r1-partition-clipping.md)。随后执行：

```text
git merge --no-ff --no-commit 580a8c3d235c22eb95b11688b509ee1e2ea65e44
```

该 SHA 是父代理提供的已发布稳定 main。没有 fetch、rebase、push、PR、archive、子代理或其他工作树修改。Theme 冲突逐段保留 main 的 Time，再追加 Descriptions；五份 golden 均由合并后的新 executable 生成，没有整份选择 ours/theirs。独立 Astra 复核由父代理安排，本记录不声称审查 CLEAN。

## 实际合并影响集合

本阶段新增 `rynui.descriptions_shared_theme`：在同一 reactive Theme 下嵌套 TimePicker，独立捕获两组件 colors/metrics 订阅，验证同时更新无 identity 冲突；不完整输入 `09:`、editor/session、slot 身份保持。真实 Time popup 的 clip boundary 越过 value 区仍可用；隐藏关闭 popup 并撤销输入资格，恢复不重挂载或复活 popup。

合并未修改 Time/Breadcrumb/Steps 的组件、model、Theme 文件和公开 API。Input 仅合入已发布 main 的 focus 查询方法；R1 内容/decoration 分层保留。受影响集合为 Descriptions 原 8 项、R1 分区裁剪 5 项、新共存 1 项、Time Theme/integration 2 项、Time/Nav merge 1 项、portable Theme algorithm/runtime 2 项及 Core/backend 边界 1 项，共 **20 项**。R1 已通过的 Input/Focus/Select/Modal/Spin 等旧集合不重复运行，无新增语义变更的完整 33 项矩阵未重跑。

所有构建使用 Windows MSVC、正式 `windows-msvc-headless` presets、Ninja Multi-Config 和 `--parallel 4`。[run-580-merge.ps1](run-580-merge.ps1) 保存目标列表、fresh golden、preservation、文档示例和 CTest 命令。实际执行顺序：

```powershell
# 新共存目标先独立运行，避免在后续 Debug 集合重复。
ctest --preset windows-msvc-headless-debug -R '^rynui.descriptions_shared_theme$' --output-on-failure
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-580-merge.ps1 -Configuration Debug -TestFilter '^rynui\.(descriptions_(model|public_api|component|theme|integration_(popup|cleanup|boundary|scene)|region_clip_(horizontal|bordered|vertical|vertical_bordered|rich))|time_picker_(theme|integration)|time_navigation_merge|portable\.(theme_(algorithm|runtime)|backend_boundaries))$'
Get-FileHash -Algorithm SHA256 design-tokens/ant-design/6.6.5/golden/*.json | ConvertTo-Json | Set-Content -Encoding UTF8 out/descriptions-580-debug-golden-hashes.json
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-580-merge.ps1 -Configuration Release
```

Debug 新共存 **1/1，0.23 秒**（`out/descriptions-580-shared-debug.log`）；其余 **19/19，6.98 秒**（`out/descriptions-580-debug.log`），合计去重 **20/20**。Release **20/20，3.55 秒**（`out/descriptions-580-release.log`）。每个配置的实际集合各运行一遍。

## Fresh golden 与旧数据保留

两个配置分别执行本树新生成的 `tests/portable/<Configuration>/rynui_portable_theme_algorithm.exe --write-goldens`。随后 [verify_preservation.py](verify_preservation.py) 严格比较字段集合、JSON 类型、数组顺序及每个旧叶子值，只允许整体 identity 和新增组件字段变化：

| 基线 | 五组旧 typed leaves | 原 IIFE 原文与顺序 |
| --- | ---: | ---: |
| 原 `62c990a` | 14032 | 28 |
| 已发布 `c083fed` | 14632 | 30 |
| 已发布 `580a8c3`（含 Time） | 15047 | 31 |
| 本树修复阶段 `ee2b9ef`（含 Descriptions） | 14872 | 31 |

最终 32 段 IIFE：main 的 31 段原样保留，Descriptions 追加 Time 后；过滤新 Time 后也保留本树旧 Descriptions block。main 全部 **177** 个 TokenIdentity 名称、数值与顺序不变，仅追加 `descriptions_colors=177`、`descriptions_metrics=178`。

Debug/Release fresh golden SHA256 逐份一致：

```text
default       31419974278DEE3256528556E34EB2310E7F4E6C063B6DA7E3B485287A729136
dark          3CC40DD529008BBD78056ED5CEA25862A5D5A235309005D03ED59A4438DF8DAE
compact       473D3DB6DCA8D2621EF02F06B388331F17084DD17DB605D18DE8F0DC083C7520
dark-compact  964B15A8DB145453F15EE6334B34968330465BC46AC71058600F676EC326E7DC
compact-dark  0D750D742D0B5CFCACCE64B31A8C01AE5D142B0E18E5069E78FC908F21AF34AD
```

源码 guard 同时精确比较 main 的静态默认配置、`ThemeScope::create_default`、heap validation snapshot、update/selfguard、descendant preflight、commit snapshot 与 Prop binding bridge；Time/Breadcrumb/Steps 源文件原文不变。相对 main 的 Gallery examples/catalog/support-overlay、父文档、renderer/platform 文件差异为空；没有修改 GPU ABI 或 stack limit。

## 收口检查与边界

```powershell
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' doctor --json
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' validate --all --strict --no-interactive
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/081-20261025-add-native-descriptions/evidence/extract-doc-examples.py
cl /nologo /std:c++20 /EHsc /utf-8 /c /Iinclude out/descriptions-doc-examples.cpp /Foout/descriptions-doc-examples.obj
git diff --check
git diff --cached --check
```

MSVC Developer PowerShell 环境由 helper 的 `Enter-VsDevShell` 建立。两段实际公开文档 C++ 示例编译退出 0；clang-format **22.1.3，668 个源码，0 失败**；doctor root healthy；strict **82/82**；diff checks 通过。tasks 4.1 在上述验收通过后恢复勾选，5/5 实施任务完成。

本机证据只覆盖 Windows MSVC headless 的平台通用逻辑，不包含 GPU 窗口、Linux、系统 IME、AT 或 Gallery 人工验收。交付后仍需父代理安排本轮全新独立 Astra 复核、整合与发布。
