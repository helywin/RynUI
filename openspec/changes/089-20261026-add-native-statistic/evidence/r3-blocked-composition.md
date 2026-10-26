# Astra R3：阻断祖先下的首次组合挂载

基线 `0634ccd4eeb997521667aa2c082996ba55584670`；worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`，分支 `t3/add-native-statistic`。只修改 Statistic/Skeleton 组合挂载实现、两份实际场景测试和本 change 证据，没有导入其他 WIP。

## 修复前复现与原因

先加入真实场景回归，保留生产代码，重新构建 Debug 后运行：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.(statistic_(model|theme|formatting|lifecycle|public_api)|skeleton_integration_retirement)$" --output-on-failure
```

结果 **4/6 通过、2 项预期失败**，退出码 8；model、Theme、formatting 与 public API 保持通过。日志：[before build](r3-before-debug-build.txt)、[before tests](r3-before-debug-tests.txt)。

1. outer `Skeleton.loading(true)` 或初始 closed Modal 下，`Statistic.loading(true)` 的富 title 先挂受控 `Select.open(true)`。内部 Skeleton 登记 input block 并同步协调，使 title Select 发出关闭请求；回调沿 Select → title region → Statistic 两级父链销毁 owner。两种祖先实测均为 `ComponentId is stale or invalid`。
2. 共享 Skeleton 的独立回归也复现相同异常：blocked outer Skeleton 下的已有 Select，在后续 inner Skeleton 首次登记 block 时关闭，回调销毁正在构造的 inner Skeleton。该 inner 的 rich body 尚未执行。

R2 的 ready 只延迟 Statistic 自己的 refresh，无法阻止子组件触发同步协调。实际续用有两层：Skeleton 的 `create` 在协调返回后继续 `mount_slot(child)` 和写 state，随后还会注册 fragment/layout；其 `mount/connect` 也会继续取已销毁 owner 的 scope。Statistic 在子组件返回后仍会写原 state、使用 Skeleton mounted 列表最后一项并安装自己后续布局/连接。

## 小范围修复与共享影响

- 保持 block 先登记及同步协调顺序；Skeleton 在协调返回、内容 slot 返回后检查 owner 与 child 的完整 ComponentId generation，在创建 fragment/layout 前再次检查 owner。若只剩失效 content handle，则清空该 handle。
- Skeleton 的 mount 与每个 connect 入口在读取 scope 前检查完整 generation；中途回调销毁 owner 后，不再安装后续连接。refresh 本身本就将可回调协调放在尾部，维持这一约束。
- Statistic 在各 rich region、内部 Skeleton、外层 slot 返回后先检查 owner generation，确认存活后才写 snapshot 或继续构造。内部 Skeleton 身份由实际组合父链记录，不再用 mounted 列表最后一项代替；若私有组合子节点被单独退役，清理未完成 owner 并停止挂载。
- 没有 try/catch 掩盖失效、没有全局协调延迟队列、没有修改 Select/Modal 规则或 ComponentHost。共享影响限于 Skeleton 在同步销毁后停止无效续用；合法存活路径、初始属性应用、block 登记与清理保持原语义。

## 实际邻接回归

- outer Skeleton 和 closed Modal 各覆盖合法存活与关闭回调销毁 Statistic；初始 loading 为 true，富 title 为 Input + 受控 Select，rich value 为另一 Input。
- 销毁时记录 rich body 声明次数，确认回调后没有继续声明旧 body；mount 返回后在仍受祖先阻断的父节点下挂新 Statistic，重用原 InputRef，解除祖先阻断后可正常聚焦，新 generation 不受旧 value binding 更新影响。
- 合法存活时富内容只挂载一次、初始输入阻断生效；祖先恢复和 loading=false 后 refs/原内容保持。controlled Select 的关闭拒绝不会自动复活，显式 false→true 可重新打开。
- 另覆盖 outer Skeleton 下单独销毁 Statistic 私有 inner Skeleton 的首次路径，确认未完成 owner 被清理，并可进行上述代际重绑。
- 共享 Skeleton 独立回归确认 inner 被销毁后 body 未执行、mounted 列表没有旧代际；在合法 mount 边界后追加新 Skeleton，其输入仍受 outer gate 管理，解除后恢复。
- Statistic 组合检查 stale owned block、旧 owner 不再存活及最终资源/ref 清理；独立共享 Skeleton 回归检查阻断、恢复和资源/ref 清理。R1 正负零原断言、R2 首次 hidden/loading 退役、字体上限及原子 preflight/恢复原断言保持并执行。

## 修复后定向验证

本机 Windows/MSVC 19.51，Ninja Multi-Config，`windows-msvc-headless`；仅使用当前 worktree 已配置构建目录，未重新 configure，未写 ROOT 或运行 ROOT/out 程序。build 每次最多 2 jobs。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_skeleton_integration_tests rynui_skeleton_component_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.(statistic_(model|theme|retained|formatting|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$" --output-on-failure
ctest --preset windows-msvc-headless-release -R "^rynui\.(statistic_(model|theme|retained|formatting|lifecycle|gates|boundaries|public_api)|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries))$" --output-on-failure
```

新的受影响集合为 **8 项 Statistic + 5 项共享 Skeleton/Select/Modal 代表检查**，不是重跑旧的完整矩阵。Debug **13/13，3.46 秒**；Release **13/13，1.90 秒**，构建和测试退出码均为 0。见 [Debug build](r3-after-debug-build.txt)、[Debug tests](r3-after-debug-tests.txt)、[Release build](r3-after-release-build.txt)、[Release tests](r3-after-release-tests.txt)。

clang-format 22.1.3 检查 736 份自有源文件，0 failures，见 [r3-format.txt](r3-format.txt)。显式 OpenSpec 入口继续使用 `C:/Program Files/nodejs/node.exe` 与 `C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js`：实测版本 **1.14.0**，doctor **healthy: true**，strict **88/88 通过、0 failed**，退出码均为 0；见 [版本](r3-openspec-version-1.14.0.txt)、[doctor](r3-doctor-1.14.0.json)、[strict](r3-strict-1.14.0.txt)。`git diff --check` 通过。

未重跑无关 full suite、旧金样、PE 或 Gallery；未修改 Theme/Token/GPU ABI。无 ROOT/Gallery 写入、远端/push/PR/archive/children 操作；HEADLESS 结果不扩大为真实窗口、GPU、Linux、系统 IME/AT。保存本地英文 Conventional Commit 后停止，交父独立 Astra R4。
