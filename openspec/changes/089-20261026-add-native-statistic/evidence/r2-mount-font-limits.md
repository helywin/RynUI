# Astra R2：首次挂载协调与字体上限

基线 `247b9bad08b052be262de8d58c44de7cfacd3e4e`；worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`，分支 `t3/add-native-statistic`。只修改 Statistic 的两个实现文件、相关测试、文档及本 change 证据，没有导入其他 WIP。

## 修复前实际复现

先添加真实回归，未修改生产代码；在当前 worktree 已配置的 MSVC HEADLESS Debug 中重新编译并运行：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.statistic_(model|theme|formatting|lifecycle|public_api)$" --output-on-failure
```

结果 2/5 通过、3 项失败，CTest 退出码 8；R1 model/public API 通过。日志：[before build](r2-before-debug-build.txt)、[before tests](r2-before-debug-tests.txt)。

- `visible(false)` 的 rich value 挂载受控 `Select.open(true)`，第一次关闭回调中销毁已登记的 Statistic；首次静态 title 连接触发输入协调，随后 prefix 连接使用失效 scope，实际抛出 `ComponentId is stale or invalid`。
- title 与 content 各自设置 `100000000` dp，实际 Theme snapshot 均先改变，然后 `FontRuntime` 通过其前置范围检查拒绝字体配置，fixture 抛出 `Input validation fonts failed to load`；两次日志均为 `snapshot changed=1`。没有尝试创建巨型 atlas。
- Theme-only 测试确认量化后的字号超出 `INT_MAX/64` 仍被旧检查接受。

红测中的替代组件最初尝试在关闭回调内 `append_slot`，触发 host 明确的 `Dynamic slots require a mounted host and cannot reenter mount` 限制。最终测试保留该 host 边界：回调只销毁 owner，首次 mount 返回后再挂载同 ref 的替代 Statistic。没有改动通用 host 以允许 mount 重入。

## 局部修复

1. StatisticState 增加仅用于挂载阶段的 `ready` 标志。初始 Prop apply 仍写入当前状态，但 `refresh` 在连接和 Theme 订阅安装完成前返回；最后设 ready 后执行首次 refresh。该输入协调返回后不再访问借用 state/scope，回调可安全销毁本 generation。没有改变 Skeleton 或全局输入协调规则。
2. 最终 title/content 字号先保留 finite、最小 0.5 dp 和组合尺寸校验，再以 `std::round(double(font_size)) <= INT_MAX/64` 检查。对应 TextComponent 的 `lround` 量化及 FontRuntime.cpp 的 `logical_pixel_size` 前置限制；不再只检查 long 最大值。此检查位于最终 derive/override 后，自动参与既有后代 preflight，不新增全库字体抽象。

## 新回归覆盖

- 首次 hidden 与首次 loading 两条关闭路径；owner 销毁、资源与 owned input block 清理；mount 返回后同 InputRef 绑定新 generation，旧 value/prefix 更新不影响新组件。原有同步 blur 卸载/ref 重绑、formatter 卸载继续执行。
- title/content 的 100000000 dp 更新在发布前抛 invalid_argument，旧 snapshot 地址、generation 和通知计数均保持；随后 16/28 dp 更新通知一次并恢复实际 retained glyph 场景。
- Theme-only 检查实际量化边界：Windows 上 `INT_MAX/64=33554431`，相邻 float `33554430` resolve 通过、`33554432` 拒绝。边界值只 resolve，不加载巨大字体或 atlas。
- 独立 seed 的算法关闭/开启合同；开启后派生 content 字号过大拒绝。祖先由 Compact 切回 Default 时，只有后代 Statistic 私有 seed 的派生字体越界，祖先/子/孙 snapshot 和通知全部保持；降低后代私有 seed 后重试祖先更新成功，三层指标通知各一次。该 fixture 不放大全局字体，避免把其他组件几何上限混入本问题。
- R1 正负零的 Signal/Binding 双向更新、静态格式化和其他 variant equality 断言原文保留，model/public API 在本轮再次执行。

## 修复后验证

本机 Windows / MSVC 19.51、Ninja Multi-Config、`windows-msvc-headless`；仅使用本 worktree 已配置构建目录，没有重新 configure，也没有访问主工作树 ROOT/out。每次 build 最多 2 jobs。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.statistic_(model|theme|formatting|lifecycle|public_api|retained|gates|boundaries)$" --output-on-failure
ctest --preset windows-msvc-headless-release -R "^rynui\.statistic_(model|theme|formatting|lifecycle|public_api|retained|gates|boundaries)$" --output-on-failure
```

受影响生产 source 和测试均重新构建。Debug **8/8 通过、2.25 秒**；Release **8/8 通过、1.34 秒**。日志：[Debug 增量 build](r2-after-debug-build.txt)、[Debug tests](r2-after-debug-tests.txt)、[Release build](r2-after-release-build.txt)、[Release tests](r2-after-release-tests.txt)。Debug 最后的增量构建日志只包含后代 preflight fixture 调整后的 Theme test 重编译，生产修复已在此前同一轮 Debug 构建完成；Release 日志包含两份生产修复 source 的编译。

clang-format 22.1.3 全仓 736 份自有源文件检查，0 failures，见 [r2-format.txt](r2-format.txt)。命令与既有格式验证一致；C++/HLSL 之外无格式批量改动。

显式 OpenSpec 入口为 `C:/Program Files/nodejs/node.exe` 加 `C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js`，执行 `--version`、`doctor --json`、`validate --all --strict --no-interactive`；版本 **1.14.0**、doctor **healthy: true**、strict **88/88 通过、0 failed**，退出码均为 0。见 [版本](r2-openspec-version-1.14.0.txt)、[doctor](r2-doctor-1.14.0.json)、[strict](r2-strict-1.14.0.txt)。`git diff --check` 通过。

没有重跑完整 13×2、旧金样、PE、长 Gallery 或历史 full suite；默认派生 token、identity 和 goldens 没有改动。无 ROOT/Gallery/远端/push/PR/archive/children 操作，不把 HEADLESS 结果扩大为 native/GPU/Linux/system IME/AT。保存本地英文 Conventional Commit 后停止，交父独立 Astra R3。
