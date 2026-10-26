# Astra R5：首次挂载的属性与生命周期衔接

## 基线和修复前实际结果

2026-10-26，原 worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`、分支 `t3/add-native-statistic`，从 clean `74c0fa48276b58e8715f4c4e0c9d50add4c014f3` 继续。本轮没有导入其他 WIP，没有访问远端或 ROOT/out 程序。

生产源码保持基线时先新增两组真实组件回归：`first_mount_updates` 在 richTitle 中更新格式属性；`final_mount_boundary` 在 `visible(false)`、loading=false 的第一次输入协调关闭 controlled Select 时，只退役私有结构或 owner，部分用例立即在稳定外部 parent 下建立替代组件。随后真实执行 `visible.set(true)` 和 value 更新。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_component_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.statistic_(formatting|lifecycle)$' --output-on-failure
```

构建退出 0，CTest **0/2、退出 8、1.37 秒**。6 个属性路径（value、precision、decimalSeparator、groupSeparator、formatter、组合 Binding）全部首帧显示旧值；最终 refresh 的私有 Skeleton、四个 region、私有 Skeleton 退役并重绑共 6 条路径实际抛 `ComponentId is stale or invalid`。owner 完整退役和合法存活对照没有上述异常。见 [r5-before-debug-build.txt](r5-before-debug-build.txt)、[r5-before-debug-tests.txt](r5-before-debug-tests.txt)。新日志规范为 UTF-8/LF、去行尾空白，保留实际输出与异常文本。

## 最小组件内修复与 first-mount 审核

原 R4 generation/父子边 helper 移为 Statistic 私有的 `valid_composition`，捕获的完整图保存在 StatisticState。挂载、getter/apply、格式化和 refresh 共用同一校验，ready 后要求完整图。失效时只清理原完整 owner generation；旧 owner 不存在或 Scope 已停用时直接返回。没有全局事务或通用 reactive/host 架构变更，也没有 try/catch 吞错。

| 会执行用户代码的衔接 | 本轮处理 |
| --- | --- |
| 初始 Prop/Binding getter | 先建立 owner 和清理入口。Statistic 本地 `watch` 在第一次 `observer->run()` 前把 observer 的 deactivate 注册到 owner Scope；getter 返回后重新校验原 generation/当前图，apply 完成和局部值释放后再校验。回调后不再访问捕获的 Scope&。 |
| LayoutStyle getter | 本地 watch 将用户源桥接为内部 Signal，再交给已有布局适配器；每个桥接 getter 均受相同校验，避免共享适配器初次订阅返回后续用已销毁 Scope/node。15 个布局字段沿用现有适配器的验证和 dirty 语义，共享适配器本身未改。 |
| 四个 rich slots 和嵌套 Content 返回 | 保留 R4 每个 region 返回、写 snapshot、下一个 mount 前的 generation/父子关系检查；图数据不再依赖 mount 栈上的引用。私有 Skeleton/wrapper/content 返回同样检查。原 16 组合全部保留。 |
| 私有 Skeleton 的 loading | 只传内部 Signal，不再传用户 getter。先用最新初始 loading 创建；其首次 mount 期间的新 loading 暂存在 owner，全部连接完成后再向内部 Skeleton 发布，避免它首次 connect 的同步协调退役自身 Scope 后继续连接。shared Skeleton 的 R3 代码保持原样。 |
| 模型初次订阅、formatter | 不再提前读取/格式化后无条件跳过首次 apply。模型各 Prop 连接吸收当前值，ready=false 时只组装模型；全部订阅建立后才首次格式化。formatter 返回后先校验；若它自身改变模型 revision，只为最新 revision 重新格式化并发布。无相关变化只调用一次 formatter。 |
| 文本/可见性及后续属性更新 | 均通过同一先拥有 observer 的 watch。动态 formatter 前复制模型，返回后重新校验和找 state，非法数值/precision 仍先拒绝并保留上一有效显示。 |
| Theme watch | 现场核对 `ThemeScope::capture`：安装时只执行固定的 typed token reads，不执行 invalidation callback。两个订阅建立后才首次 formatter/ready；Theme 更新触发的 refresh 也走统一图校验。 |
| ready、首次 loading 同步、最终 refresh 和 RAII 返回 | ready 之前检查候选模型/formatter 局部副本已经离开作用域后的存活情况；ready 后同步 loading，返回后检查；refresh 的语义 Signal 更新、最终输入协调返回均校验，mount 最后也校验一次。仅保留内部 Signal、ComponentId、服务引用等局部 handles，不跨用户回调读取初始化的 s/state/scope。 |

公开文档补充 slot 期间格式属性更新反映到首帧，以及稳定首次 formatter 只调用一次；API 示例没有修改。

## 定向新增断言

- 首帧属性 6 个用例：4 个直接 Signal 数值/精度/分隔符路径、formatter Binding、组合 Binding；之后合法更新、非法 finite 拒绝、有效恢复，并保留富 Input 草稿/ref。另有静态 formatter 首次只调用一次、formatter 改 value 时仅最新 revision 发布两个断言。
- 最终边界 10 个用例：四个 region、私有 Skeleton、完整 owner、Skeleton/owner 回调同 index 新 generation 重绑、合法存活，以及 richValue 把初始 loading=false 改为 true 后由最后同步触发私有 Skeleton 退役。
- getter/formatter 11 个定向用例：初始 formatter 的 owner/Skeleton/region 退役、模型 getter 的 owner/Skeleton 退役、早期 visible/layout getter、文本 getter、挂载中 loading getter，以及 ready 后 formatter/getter 退役私有结构；均回调建立同 ref 的同 index 新 generation，断言原 owner 清理、新组件值仍为 99、draft/ref/input blocks/surfaces 正确。
- 额外存活路径：richValue 挂载中改变 loading，首帧内部 Skeleton 和输入阻断一致，解除后原 Input 可恢复。

这些用例并入现有 formatting/lifecycle 项；R1 signed zero、R2 ready/字体 preflight 原子性、R3 outer Skeleton/closed Modal、R4 四 region 16 组合断言全部保留。

## Fresh Debug / Release

本 worktree 现有 `windows-msvc-headless` 配置，Windows / MSVC 19.51.36256 / Ninja Multi-Config，最多 2 jobs。只编译、运行自己的受影响目标；本轮未重新 configure、未扩栈、未重生成 Theme 金样。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_statistic_model_tests rynui_statistic_theme_tests rynui_skeleton_component_tests rynui_skeleton_integration_tests
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_statistic_model_tests rynui_statistic_theme_tests rynui_skeleton_component_tests rynui_skeleton_integration_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(statistic_(model|theme|retained|formatting|clips|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.(statistic_(model|theme|retained|formatting|clips|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
```

构建均退出 0。Debug **14/14，7.79 秒**；Release **14/14，4.32 秒**。9 项 Statistic 含实际 glyph/quad/hit clip；5 项直接共享 Skeleton 代表为 component 与 editor/select/retirement/boundaries integration。见 [r5-after-debug-build.txt](r5-after-debug-build.txt)、[r5-after-debug-tests.txt](r5-after-debug-tests.txt)、[r5-after-release-build.txt](r5-after-release-build.txt)、[r5-after-release-tests.txt](r5-after-release-tests.txt)。Debug 日志保留分步增量重编译及最后 no-work 确认；中间两项转绿/扩展 callback 回归记录也保留，不充当最终源码的完整结果。

## 工具与移交

```powershell
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' --version
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' --version
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' doctor --json
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' validate --all --strict --no-interactive
git diff --check
git diff --cached --check
```

clang-format **22.1.3，736 文件，0 failures**，见 [r5-format.txt](r5-format.txt)。现场确认 cmd 和 node+js 两个入口都为 **1.14.0**，见 [r5-openspec-cmd-version-1.14.0.txt](r5-openspec-cmd-version-1.14.0.txt)、[r5-openspec-version-1.14.0.txt](r5-openspec-version-1.14.0.txt)。指定 node+js 的 doctor 返回本 worktree 根、`healthy: true`，strict **88 passed / 0 failed**，均退出 0；见 [r5-doctor-1.14.0.json](r5-doctor-1.14.0.json)、[r5-strict-1.14.0.txt](r5-strict-1.14.0.txt)。PATH 1.13.1 是早期历史，不是本轮缺失条件，也未尝试联网安装。

最终 working/cached `git diff --check` 均无输出、退出 0，tasks 8.1 在上述实测通过后勾选。

没有改共享 Skeleton/Select/Modal、ROOT、Gallery 或其他父拥有文档/生成目录，没有 remote/push/PR/archive/children。结果仅表示本机 MSVC HEADLESS 逻辑场景与路由通过，不代表 native window、GPU、Linux、system IME/AT。英文本地 Conventional Commit 后停止，交父 fresh 独立 R6。
