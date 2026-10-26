# AutoComplete 实施证据

## 基线与规划

本树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-081ff944` 从 `83d26e7a16a0590c2d52867296a20dd3cf4aee78` 开始，规划 commit `27cdcbe8`。干净阶段显式 merge 父指定 `6118f67728aede9985867377b122274ac86f31d7`，merge commit `2c6dbba2a005a39ef487714bf4b0198cf353969f`。没有 fetch/rebase/push/PR/archive/children，没有手工创建 branch/worktree。app 当前 branch 为 `t3/add-native-autocomplete`。

锁定上游只读 `D:/code/RynUI/out/upstream-ant-design-6.6.5` HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`；阅读 AutoComplete.tsx、index.en-US.md、__tests__/index.test.tsx、demo/basic/options/non-case-sensitive。Context7 查得当前说明作补充，行为以指定本地版本为准。

## 阶段 2：模型与 Theme

Windows MSVC 19.51.36256.0、Ninja Multi-Config、`windows-msvc-headless`、BUNDLED，`--parallel 2`。配置 helper `out/092-msvc.ps1` 引入 VSDevShell，仅引用 ROOT/out 的四个锁定第三方 source；字体复制到本树，所有输出/cache/log 都在本树。配置第一次因 PowerShell 将 HarfBuzz 的 CMake warning 当作终止错误而退出；调整外部命令错误流处理后配置 exit 0。没有修改全局工具或第三方源。

实际构建 targets 为 `rynui_auto_complete_model_tests rynui_auto_complete_theme_tests`，Debug/Release 各 194 个 Ninja 步骤、exit 0。`ctest --preset windows-msvc-headless-{debug,release} --parallel 2 --output-on-failure -R '^rynui\.auto_complete_(model|theme)$'` 均 2/2，Debug 0.24 s，Release 0.07 s。覆盖自由输入、默认不过滤、predicate、禁用跳过、受控权威、backfill、key 重排、非法 key/predicate 拒绝；Theme 覆盖 recipe、独立 algorithm、inherit、hash/equality、material/metrics dirty、font round 上下限、非法字体和 descendant preflight 原子性及恢复。

两配置 Theme 测试各 fresh 生成五份 goldens；`tests/verify_auto_complete_theme_baseline.py out/092-goldens-Release` 通过：Avatar 集成基线的 37 个旧 IIFE 原文/顺序（含原始 36 个）、198 个旧 TokenIdentity ordinal（原始 196 + Avatar 2）和五份旧 JSON 全部嵌套 typed leaves 不变，只追加 AutoComplete。五份 repo goldens 来源为本树 Release fresh 输出。

原始日志：`out/092-configure-Debug-attempt1.log`、`out/092-configure-Debug.log`、`out/092-build-{Debug,Release}.log`、`out/092-stage2-test-{Debug,Release}.log`、`out/092-goldens-{Debug,Release}.log`、`out/092-stage2-baseline.log`、`out/092-stage2-format.log`。现有第三方 C4819 及自有既存 warning 保留。OpenSpec 显式 1.14 doctor healthy、strict 90/90，diff check 通过；format 使用 clang-format 22.1.3 显式路径，已跟踪 731 个文件 0 failures，新增文件另已直接格式化（收口时纳入完整检查）。

此阶段没有把尚未链接的原生 host 计作完成；原生事件、Form、popup、生命周期和示例仍待后续阶段。
