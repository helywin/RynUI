# Astra R4：富 region 返回后的组合一致性

## 范围与实际复现

2026-10-26，在原隔离工作树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`、分支 `t3/add-native-statistic`，从 clean `263bba4f479d72bab6668953364e384ce3d8f6a3` 继续。本轮只修改 Statistic 挂载逻辑、其真实组件回归与本 change 证据；共享 Skeleton 的 R3 守卫和 block 释放代码保持不变。

先在生产代码未修改时新增 `rich_region_retirement()`。实际路径为初始 `Statistic.loading(true)`，目标 rich slot 依次挂 Input、controlled `Select.open(true)`、带内容的 `Skeleton.loading(true)`；后者新增输入 block 同步关闭 Select，回调通过公开组件父链销毁目标。title 使用初始 loading 的外层 Skeleton 形成阻断祖先。没有拦截或吞掉生产异常。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(statistic_(model|theme|formatting|lifecycle|public_api)|skeleton_integration_retirement)$' --output-on-failure
```

构建退出 0；修复前 CTest **5/6 通过，lifecycle 失败，退出 8，2.66 秒**。日志实际出现 9 条 `ComponentId is stale or invalid`：四个当前 region 退役、prefix/value 私有 Skeleton 退役、prefix wrapper 退役、value content 退役、prefix 私有 Skeleton 退役后在回调中建立替代组件。suffix 私有 Skeleton 退役已有晚期守卫能停下，本轮仍纳入统一回归。见 [r4-before-debug-build.txt](r4-before-debug-build.txt)、[r4-before-debug-tests.txt](r4-before-debug-tests.txt)。本轮新日志统一保存为 UTF-8、去行尾空白，保留实际输出与异常文本。

## 局部修复与覆盖

`StatisticComponentHost::mount` 用局部 `valid_mount` 保存并检查完整 ComponentId（含 generation）及父子边：owner 的原 parent、title→owner、私有 Skeleton→owner、wrapper→Skeleton、content→wrapper、prefix/value/suffix→content。每个已建立的 region 在用户 slot 返回后、写 snapshot 前、开始下一个 mount 前都检查；组合完成时还要求所有私有节点与四个 region 齐全。连接属性时各可回调边界也检查完整组合。

私有结构退役而原 owner 仍存活时，仅销毁捕获的未完成 owner 并返回；原 owner 已退役时直接返回。判断和销毁均使用原 generation，不会触碰回调建立的新 generation。提前复制文本/语义样式 Signal handles，回调后重新查找 state；不再从初始化时的 `s` 或借用 scope 续读 snapshot/信号。没有全局挂载架构变更或 try/catch 补丁。

新增 **16 个实际组合**，集中在现有 lifecycle 项内：

- 四个 richTitle/prefix/value/suffix 的当前 region 被销毁。
- prefix/value/suffix 中私有 Skeleton 被销毁；prefix 的 wrapper、value 的 content 被销毁，原 owner 当时仍活着。
- prefix 中全 owner 销毁；value 全 owner 销毁和 prefix 私有 Skeleton 销毁后，回调用当前构建上下文 `mount_slot` 在稳定外部 parent 下建立替代 Statistic。断言替代组件复用退役目标的 index、generation 不同，旧挂载检查不会销毁它。
- 四个 rich region 的合法存活/恢复路径，确保富 slot 各执行一次、draft/ref 保留。

退役场景同时断言停止后续 region、旧 owner 完整清理、旧 value Signal 不修改替代组件、所有登记 block 的完整 ComponentId 仍活着；解除 ancestor/loading 后实际 focus 与 `TextCommitted` 输入可用，dispose 后 ref/blocks/surfaces 清空。保留 R1 signed zero、R2 首次 ready/字体 preflight 原子性与恢复、R3 outer Skeleton/closed Modal 首次挂载及共享 Skeleton 直接回归。

## 修复后定向检查

在本 worktree 已配置的 `windows-msvc-headless` 中用 Visual Studio 2026 x64 Developer PowerShell（MSVC 19.51.36256）、Ninja Multi-Config 构建，最多 2 jobs；运行本 worktree 自己的可执行文件。本轮没有重新 configure 或访问 ROOT/out。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(statistic_(model|theme|retained|formatting|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.(statistic_(model|theme|retained|formatting|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
```

- Debug fresh 受影响构建退出 0，**13/13 通过，6.23 秒**。其中 lifecycle 3.47 秒。见 [r4-after-debug-build.txt](r4-after-debug-build.txt)、[r4-after-debug-tests.txt](r4-after-debug-tests.txt)。Debug 构建日志保留修复编译及增强同 index/generation 断言后的增量重编译。
- Release fresh 受影响构建退出 0，**13/13 通过，2.90 秒**。其中 lifecycle 1.65 秒。见 [r4-after-release-build.txt](r4-after-release-build.txt)、[r4-after-release-tests.txt](r4-after-release-tests.txt)。
- 上述集合为 8 项 Statistic、5 项共享 Skeleton（component、editor/select/retirement/boundaries integration）直接代表检查。R3 的 closed Modal 与 Select 控制拒绝路径也在 Statistic lifecycle 内实际执行。

## 工具验收与交接边界

```powershell
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' --version
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' doctor --json
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' validate --all --strict --no-interactive
git diff --check
git diff --cached --check
```

format：clang-format **22.1.3，736 文件，0 failures**，见 [r4-format.txt](r4-format.txt)。显式 node+js 入口确认 **1.14.0**；doctor 根为本 worktree、`healthy: true`；strict **88 passed / 0 failed**，均退出 0。见 [r4-openspec-version-1.14.0.txt](r4-openspec-version-1.14.0.txt)、[r4-doctor-1.14.0.json](r4-doctor-1.14.0.json)、[r4-strict-1.14.0.txt](r4-strict-1.14.0.txt)。working/cached 两次 diff check 均无输出、退出 0。历史 PATH 1.13.1 证据保留，不充当本轮结果。

本轮不改 Theme/token，所以不重生成金样；不扩大 PE 栈，也不重跑无关矩阵、历史 full suite 或 Gallery。结果限本机 MSVC HEADLESS 逻辑场景，不代表 native window、GPU、Linux、system IME/AT。保存英文 Conventional Commit 后停止，交父 fresh 独立 R5；没有执行 remote/push/PR/archive/children。
