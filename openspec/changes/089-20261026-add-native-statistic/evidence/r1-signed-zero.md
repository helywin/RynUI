# Astra R1：正负零响应更新修正

基线：`1990c276bc40f613666ed9b5babe0d7a96487c44`，worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`，分支 `t3/add-native-statistic`。仅修改 StatisticValue equality、两份既有定向测试与本 change 证据；没有导入其他 WIP。

## 修复前复现

先添加测试，保留原默认 equality，使用当前 worktree 的 MSVC HEADLESS Debug 重新编译受影响测试，再运行以下集合：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.statistic_(model|formatting|public_api)$" --output-on-failure
```

结果 1/3 通过、2 项预期失败，退出码 8。model 报告 `opposite-sign double zeros compare equal`；public API 实际挂载分别从正零与负零开始的两个组件，复现以下四条错误；原 formatting 项通过：

```text
Binding update to -0 remained 0
Signal update to -0 remained 0
Binding update to +0 remained -0
Signal update to +0 remained -0
```

Binding 使用审查提供的 `Signal<bool>` 条件生成 `StatisticValue{negative.get() ? -0.0 : 0.0}`，Signal 路径使用 `Signal<StatisticValue>`。见 [修复前构建](r1-before-debug-build.txt) 和 [修复前测试](r1-before-debug-tests.txt)。

## 最小修复与回归合同

`StatisticValue::operator==` 先保留原 variant 比较；原比较相等且当前 alternative 为 double 零时，再比较 `std::signbit`。不同 variant、非零值、String 与 NaN 的原比较语义保持；格式化和组件 binding 无需改动。

- model 覆盖正负零的静态输出（`0` / `-0`、precision=2 的 `0.00` / `-0.00`）、同符号零相等、不同符号零不等；验证 int64/uint64/double/String alternative 的相同值及类型区别，以及非零 double、不同 String、NaN 比较语义。
- public API 对正零和负零两种初始状态分别往返更新，检查 Binding 与 `Signal<StatisticValue>` 两条路径的两方向输出及 component count 保持；原文档示例与数值 Signal/Binding 用例同时执行。
- 复用原 component formatting 项，覆盖 precision、分隔符、formatter、非法数值更新恢复及实际 glyph 场景。

## 修复后新构建与结果

同一机器 Windows / MSVC 19.51、Ninja Multi-Config、`windows-msvc-headless`，每次 build 最多 2 jobs。仅使用本 worktree 已配置构建目录，未重新 configure 或访问主工作树 ROOT/out。修复后重新编译三个目标及受影响的生产 Statistic source。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
ctest --preset windows-msvc-headless-debug -R "^rynui\.statistic_(model|formatting|public_api)$" --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_model_tests rynui_statistic_component_tests rynui_statistic_public_api_tests
ctest --preset windows-msvc-headless-release -R "^rynui\.statistic_(model|formatting|public_api)$" --output-on-failure
```

Debug **3/3 通过，0.41 秒**；Release **3/3 通过，0.30 秒**，构建与测试退出码均为 0。日志：[Debug build](r1-after-debug-build.txt)、[Debug tests](r1-after-debug-tests.txt)、[Release build](r1-after-release-build.txt)、[Release tests](r1-after-release-tests.txt)。未重跑完整 13×2、金样、PE、Gallery 或历史 full suite。

```text
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
```

实际 Python 为 `C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`；clang-format 22.1.3，736 份自有源文件检查，0 failures，见 [r1-format.txt](r1-format.txt)。

显式 CLI 使用 `C:/Program Files/nodejs/node.exe` 与 `C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js`，在当前 worktree cwd 分别执行 `--version`、`doctor --json`、`validate --all --strict --no-interactive`。实测版本 **1.14.0**，doctor `healthy: true`，strict **88/88 通过、0 failed**，退出码均为 0。见 [版本](r1-openspec-version-1.14.0.txt)、[doctor](r1-doctor-1.14.0.json)、[strict](r1-strict-1.14.0.txt)。`git diff --check` 通过。

仅保存本地英文 Conventional Commit；无 ROOT/Gallery 修改、远端访问、push、PR、archive 或 children。HEADLESS 证据不扩大为 GPU/native/Linux/system IME/AT 通过。修复后交父线程独立 Astra R2，worker 不追加功能。
