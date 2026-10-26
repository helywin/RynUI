# Astra R6：局部 catch 的首次挂载回滚

## 基线、宿主合同与修复前实测

原 worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`，分支 `t3/add-native-statistic`，clean 起点/本轮提交 parent 为 `ca9542ba0a22c74bff5a460b64062f77d7ff093c`。未导入其他 WIP。

核对 `ComponentHost::mount`：仅当异常逃出整个 Content 时 dispose 宿主；`append_slot` 同样只在异常逃出动态 Content 时删除新 children；`mount_slot` 的 ActiveBuildContextGuard 只恢复上下文，不回滚局部组件。`WindowComponentServices::mount/append_slot` 恢复参与者与挂载入口，但不能看到被应用局部 catch 的异常。相邻组件没有可直接复用的通用 mount RAII guard，因此本轮仅在 Statistic 内增加局部 guard。

先保持生产源码为 parent，只新增 `rynui.statistic_rollback`，在成功返回的窗口 Content 中 try Statistic、局部 catch，再挂 fallback Input；不会借宿主的异常回滚来清理失败的 Statistic。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_component_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.statistic_rollback$' --output-on-failure
```

构建退出 0。红测 **0/1、退出 8、0.78 秒**，11 个场景中 8 个失败：Infinity、NaN、throwing formatter 均实际保留 `owner_alive=1 statistic=1 inputs=1 cleanup=0`；NaN 初始 loading=true 还保留 `blocks=1`。前三项已经先操作 fallback、提交真实文本并确认 glyph 可见，再发现旧资源/订阅残留。getter、slot、early getter、私有 Skeleton 重绑后抛错和首次 refresh 抛错也暴露残留/布局失效；owner 已完整销毁的重绑对照原本可通过。见 [r6-before-debug-build.txt](r6-before-debug-build.txt)、[r6-before-debug-tests.txt](r6-before-debug-tests.txt)。新日志保存为 UTF-8/LF、去行尾空白，保留实际输出与异常文本。

## 局部修复和新断言

`StatisticComponentHost::mount` 在取得新 owner 的完整 ComponentId 后立即创建栈上 `MountRollback`，早于 root/state、getter、布局、slots、订阅、formatter 和首次输入协调。仅在最终 `valid_mount(true)` 成功后解除；异常或未完成返回都通过既有 `WindowComponentServices::destroy` 清理捕获的原 generation。没有公共 API、全局事务或共享 Skeleton 修改。

挂载异常本身不被捕获，按原类型和消息继续传播给应用 catch。guard 的 noexcept 析构只隔离清理期间的次生异常：现有 destroy 在所有记录/资源退休、participants 清理后才进入可能执行用户代码的 boundary reconciliation，次生异常不能替换原挂载异常。destroy 始终接收原完整 generation，owner 已销毁且 index 被复用时是安全 no-op。

新单项包含 11 个实际场景，均局部捕获后继续成功挂载 fallback：

- Infinity、NaN（初始 loading，检验 owned block）、首次 formatter 主动抛错。
- 订阅初次 value getter、richValue slot、早期 visible getter 主动抛错。
- formatter 在抛错前销毁完整 owner 或私有 Skeleton，并在稳定外部 parent 下以同 ref 创建同 index、新 generation 的替代 Statistic。
- 初次 refresh 关闭 controlled Select 时用户回调抛错。
- value getter、richValue slot 在抛错前销毁 owner 并重绑后抛错。

断言原 invalid_argument 或自定义异常类型/消息保留；捕获点组件/node/surface/interaction/block/Input/Text/Theme 订阅计数返回基线（重绑场景保留替代组件）；旧真实 input session 终止、旧 ref 注销；fallback focus、TextCommitted 与实际 glyph 可用；随后旧 value Signal 合法更新不会重新读 getter 或调用 formatter。旧 node generation 全部消失；旧 owner 和非 Text 子布局、Text intrinsic 在正确清理阶段已注销；替代值仍为 99，草稿/ref 可用；最终 dispose 后所有组件、node、surface、interaction、block、session/ref、Theme 订阅回零。

### 布局探针修正记录

首次修复后的 Debug 代表集合中，原有 14 项全部通过；新 rollback 项的 6 个断言失败于测试把所有已退休 Text 的 LeafLayout metadata 也当作资源泄漏。单项诊断实测为 `alive=0 layout=1 intrinsic=0 text=1`。现场读取 Text 创建/清理和 LayoutEngine：普通 Text 使用无捕获的 LeafLayout，并在资源清理时移除 intrinsic/text scene；布局 slot 保留普通值缓存，死 node generation 不能测量/呈现它。

只修正新测试：在 owner 的资源清理钩子中（子资源已清理、node 尚存活）检查 intrinsic 与持有回调的子布局；随后检查 owner 布局和所有旧 node。没有为测试扩大共享 Text/布局架构，也没有放过 live node 或持有回调的 Statistic 布局。保留 [r6-after-debug-tests.txt](r6-after-debug-tests.txt) 的 **14/15、8.76 秒** 和 [r6-layout-diagnostic-tests.txt](r6-layout-diagnostic-tests.txt) 的诊断红测；生产 guard 自此未改变。

## 最终受影响检查

Windows / MSVC 19.51.36256，当前 worktree 自己的 `windows-msvc-headless`、Ninja Multi-Config，最多 2 jobs。新增 CTest 项触发现有 build 目录自动 regenerate，使用既有锁定 source/cache；日志保留 HarfBuzz 原有 CMake 警告，不声明无警告，也未访问远端。未运行 ROOT/out 可执行文件。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_statistic_model_tests rynui_statistic_theme_tests rynui_skeleton_component_tests rynui_skeleton_integration_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(statistic_(model|theme|retained|formatting|clips|lifecycle|gates|boundaries|rollback|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_component_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.statistic_rollback$' --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_statistic_model_tests rynui_statistic_theme_tests rynui_skeleton_component_tests rynui_skeleton_integration_tests
ctest --preset windows-msvc-headless-release -R '^rynui\.(statistic_(model|theme|retained|formatting|clips|lifecycle|gates|boundaries|rollback|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$' --output-on-failure
```

- Debug：同一最终 guard 源码的 **14 项代表全部通过**；仅补跑修正后的新 rollback 项，**1/1、0.76 秒、退出 0**，内含 11 个场景；不重复已通过的其余 14 项。见 [r6-after-debug-build.txt](r6-after-debug-build.txt)、[r6-rollback-debug-tests.txt](r6-rollback-debug-tests.txt)。不是宣称某次单独 Debug CTest 为 15/15。
- Release：最终 fixture 与 guard fresh 构建退出 0，集合一次 **15/15、5.87 秒、退出 0**；其中 rollback 0.53 秒。见 [r6-after-release-build.txt](r6-after-release-build.txt)、[r6-after-release-tests.txt](r6-after-release-tests.txt)。
- 原 9 项 Statistic 代表保留 R1 signed zero、R2 字体/ready、R3 祖先阻断/同步销毁、R4 四 region 16 组合、R5 最终 refresh/最终 Prop 等全部原断言；5 项直接共享代表为 Skeleton component 与 editor/select/retirement/boundaries integration。

## 工具与移交边界

```powershell
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' --version
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' doctor --json
& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js' validate --all --strict --no-interactive
git diff --check
git diff --cached --check
```

clang-format **22.1.3，736 文件，0 failures**；见 [r6-format.txt](r6-format.txt)。显式入口现场确认 **1.14.0**，doctor 为本 worktree 根且 `healthy: true`，strict **88 passed / 0 failed**，均退出 0。见 [r6-openspec-version-1.14.0.txt](r6-openspec-version-1.14.0.txt)、[r6-doctor-1.14.0.json](r6-doctor-1.14.0.json)、[r6-strict-1.14.0.txt](r6-strict-1.14.0.txt)。

working/cached diff check 均无输出、退出 0。tasks 9.1 在实测后勾选，再次调用 `instructions apply --change 089-20261026-add-native-statistic --json` 确认 **11/11、remaining=0、all_done**，摘要见 [r6-apply-complete.json](r6-apply-complete.json)。

不改 GPU ABI、公共 API、Theme/token、共享 Skeleton、ROOT、Gallery、README 或全局进度；不重跑历史 full suite/金样/无关矩阵，不扩栈。证据只表示本机 MSVC HEADLESS 逻辑场景与路由，不表示原生窗口/GPU/Linux/system IME/AT。只保存本地英文 Conventional Commit，返回 clean HEAD/parent 后停止交父 fresh 独立 R7；无远端/push/PR/archive/children。
