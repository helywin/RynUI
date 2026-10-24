# 083 独立合并记录：main 22ee615

本次只整理父基线，不新增功能。原 083 proposal/design/spec/tasks 和既有 evidence 全部保留 R1 目标的原 blob，原 `tasks.md` 仍为历史 **5/5**。以下是独立合并检查清单，不回退或重记原实施任务。

## 合并检查清单

- [x] 核实干净 R1 HEAD、固定 main 对象、共同基线与实际路径边界，确认无需修改 Skeleton/Core 实现。
- [x] 使用显式非 fast-forward merge；确认无冲突，并对完整合并树做双亲原文/blob 并置校验。
- [x] 复用既有源代码和二进制证据；本树现有 Debug/Release 各执行 model、component、Theme 三项短代表测试，核对六个二进制 SHA256 不变。
- [x] 新增本记录及保留校验脚本；doctor、strict、diff 检查通过，供本地英文双亲 merge commit 交接。

## 固定对象与实际改动

- 共同基线：`580a8c3d235c22eb95b11688b509ee1e2ea65e44`。
- 第一亲 / Skeleton R1：`9a83644c41d6db9d82a6a63e662a8cb207b295ab`。
- 第二亲 / 已发布 main：`22ee615347967dc204d89ff3508b74e63c68ad5e`。
- 相对共同基线，R1 改动 **38** 个路径，main 改动 **10** 个路径，交集为 **0**。main 没有改变 `src/`、`include/`、`cmake/`、`design-tokens/`、CMakeLists/presets 或 OpenSpec config。
- main 的 10 个路径仅为 `docs/component-completion.md`、`docs/form.md`、`docs/time-picker.md`，三个 `examples/token_gallery/` 文件、support overlay、079 的 stage3 evidence/tasks，以及 `tests/token_gallery_frame_tests.cpp`。
- `git merge --no-ff --no-commit 22ee615347967dc204d89ff3508b74e63c68ad5e` 自动合并成功，**无冲突、无手工生产源码改写、无 rebase**。本次额外文件只有本记录和 `verify-main-22ee615.py`。

## 双亲保留证据

`verify-main-22ee615.py` 从 Git 对象读取完整树，按共同基线与两个互不重叠的 delta 构造预期树，再逐路径比较 mode/type/blob。除上述两个新增合并证据文件外，**8,840** 个文件组成精确并置：

- **8,830** 个非 main delta 路径保留 R1 blob，包含全部 Skeleton 生产实现、公开头、模型、测试、完整 Theme/CMake/窗口服务接入、goldens 和原 083 规划/5 项任务/历史证据。
- **8,802** 个非 R1 delta 路径保留 main blob，包含全部父 README、目录、完成记录、父文档和原生 Gallery 源码；其中全部 **51** 个 `examples/`、`gallery/` 文件逐 blob 与 main 相等。
- **15** 个共享 Core/API/Theme/build 路径均证明 `main == 580a8c3`，而合并结果 `== R1`，只保留原有 Skeleton 接入。完整路径清单保存在本树 `out/083-merge-22ee615/` 的 JSON 报告中。
- 父 079 evidence 中的 `stable_ids=334` 及全部 Time/Nav/Form/Date Gallery 定义、目录、overlay、测试原文被完整继承；本次没有重新执行 runtime Gallery ID 计数或长 Gallery flag，不把源码保留当作新增原生验收。
- 运行原 `verify-baseline.py` 再次确认 **15,047** 个旧 scalar typed values、**31** 个旧 IIFE 原文与顺序不变；五份 Skeleton goldens 保持 R1 blob/hash，TokenIdentity 仍仅原先的 Skeleton 两项追加。原 default scope、heap descendant preflight、config self guard、Time/Prop bridge 源文保持。

本记录按父线程本次通知关联：Skeleton 全新 Astra R1 对 `9a83644` 报告 CLEAN、未发现 P1/P2，独立 Debug/Release 各 19/19；main 的 079 Time Gallery 已 Astra R4 CLEAN。此处是对父通知的归档，不声称本线程重新进行了这两次独立审查。

## 复用与本次实际执行

原实施验收的 **51×2**、父通知的 R1 **19×2** 均未重跑。完整树/blob 检查证明 Core、API、Theme、goldens、CMake/config 和全部 Skeleton 测试与 R1 完全相同，因此复用本树现有 MSVC `windows-msvc-headless` 缓存。没有重新 configure/build，没有下载 native 依赖，没有使用或写入 ROOT/out。

本次只实际新执行：

```powershell
git merge-base 9a83644c41d6db9d82a6a63e662a8cb207b295ab 22ee615347967dc204d89ff3508b74e63c68ad5e
git diff --name-status 580a8c3d235c22eb95b11688b509ee1e2ea65e44 22ee615347967dc204d89ff3508b74e63c68ad5e
git diff --quiet 580a8c3d235c22eb95b11688b509ee1e2ea65e44 22ee615347967dc204d89ff3508b74e63c68ad5e -- src include cmake CMakeLists.txt CMakePresets.json design-tokens
git merge --no-ff --no-commit 22ee615347967dc204d89ff3508b74e63c68ad5e
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/083-20261025-add-native-skeleton/evidence/verify-main-22ee615.py --output out/083-merge-22ee615/before.json
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/083-20261025-add-native-skeleton/evidence/verify-baseline.py
ctest --preset windows-msvc-headless-debug -R '^rynui\.skeleton_(model|component|theme)$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.skeleton_(model|component|theme)$' --output-on-failure
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/083-20261025-add-native-skeleton/evidence/verify-main-22ee615.py --output out/083-merge-22ee615/after-tests.json
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' doctor --json
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' validate --all --strict --no-interactive
git diff --check
git diff --cached --check
```

实际新结果：Debug **3/3**（0.48 s），Release **3/3**（0.20 s）；doctor root healthy、strict **82/82**，unstaged/staged diff 检查通过。日志在本树 `out/083-merge-22ee615/{Debug,Release}-ctest.log`，全部 source/blob 与二进制报告在同目录。六个代表二进制执行前后的 size/SHA256 完全相同：

| 配置 / 目标 | SHA256 |
| --- | --- |
| Debug / model | `0c8309273b8763dcb0f6e726f6584623d1663770c94a812e978121bd811eb12c` |
| Debug / component | `f3753bc08a216145ca29669258c0afe844955640f079693c5af21378b849cb03` |
| Debug / Theme | `3cb0c14850e891d508580c88201863b8fcefcc620514bc9807ee2bba47bfc2df` |
| Release / model | `0d676fc0e0cc8df8076857674b01c26c4e7a612629f4f5e25deb5e6d4be4fd95` |
| Release / component | `dec87619bac4a42e8f19db02af3a8639378848a755c359d7a138e7e6df45fb5c` |
| Release / Theme | `043f746ed48bbff8d0caa0dcdde9dd94786ea0a68ca550420d693866a46d2714` |

## 交接边界

本线程没有修改 global Gallery/README/overlay/父 docs，仅通过固定 main 的干净 merge 继承。没有 push、PR、archive、children、ROOT checkout 或 ROOT/out 操作。父线程的完整 9 项与 native 启动验收仍由父线程负责，本记录不替代该实际集成检查。提交后可用 `verify-main-22ee615.py --ref HEAD` 再核实双亲恰为 R1/main 及完整 blob 并置。
