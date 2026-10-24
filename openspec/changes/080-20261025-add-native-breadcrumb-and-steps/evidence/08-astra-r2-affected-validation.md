# Astra R2 生命周期修复：受影响验收与 R3 交接

## Scope / Results

修复提交：`c5e0378`，基于干净的 `5f71281a04c130949c33bc23506d263d7f6957c0`。只在原 worktree / branch 工作，没有合并父 main、TimePicker 或 Gallery WIP。

新生命周期回归的 before/after 和清理顺序见 07-astra-r2-block-lifetime.md：修复前 Debug/Release 均 10/10 失败，8 轮后 block 总数/基线为 9/1 或 17/1；修复后每配置 **10/10**，每个 case 均完成 **8 轮**同窗口/registry 循环并精确回到 block 基线，保留其他 owner 和存活原生输入行为。

本阶段不重复全 46 项；每配置只执行下列 **24** 个既有受影响 checks **一次**。新 10 项已在 focused 阶段执行，不混入此集合：

- 原 Breadcrumb/Steps component tests 与 R1 五项 child pointer/native Input/focus-outline coverage cases（共原导航 7 cases）。
- 两组件 Theme、API/model、可编译运行的文档示例与 snippet 一致性。
- Feedback/Spin、Theme algorithm/runtime/default/scope、Button、Focus order/state/lifecycle、Modal component/boundary、Input。

实际 Windows / MSVC 19.51.36256 / Ninja Multi-Config / windows-msvc-headless（HEADLESS/RECORDING），所有 build 上限 4 jobs；未更改 stack。结果：

| Set | Debug | Release |
| --- | --- | --- |
| 新 lifecycle focused | 10/10，1.28 s | 10/10，0.58 s |
| 既有 affected checks | 24/24，15.04 s | 24/24，7.55 s |

## Exact affected commands

在 docs/development/building.md 的 x64 VS 环境、UTF-8 控制台和 `VSLANG=1033` 执行：

```powershell
$targets = @(
    'rynui_navigation_model_tests',
    'rynui_navigation_examples_tests',
    'rynui_navigation_review_tests',
    'rynui_breadcrumb_component_tests',
    'rynui_breadcrumb_theme_tests',
    'rynui_steps_component_tests',
    'rynui_steps_theme_tests',
    'rynui_feedback_component_tests',
    'rynui_portable_theme_algorithm',
    'rynui_portable_theme_runtime',
    'rynui_portable_default_theme',
    'rynui_portable_theme_scope_component',
    'rynui_portable_button_component',
    'rynui_portable_focus_order',
    'rynui_portable_focus_state',
    'rynui_portable_focus_lifecycle',
    'rynui_portable_modal_component',
    'rynui_portable_modal_boundary',
    'rynui_portable_input_component'
)
cmake --build --preset windows-msvc-headless-debug --target $targets --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui\.(navigation_model|navigation_examples|navigation_docs_examples|navigation_review_[a-z_]+|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|feedback_component|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|input_component))$' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target $targets --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui\.(navigation_model|navigation_examples|navigation_docs_examples|navigation_review_[a-z_]+|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|feedback_component|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|input_component))$' --output-on-failure
```

实际摘要日志（ignored）：`out/080-r2-affected-debug.log`、`out/080-r2-affected-release.log`。新 focused 命令及日志见 07。

## Golden / Docs / Checks

两个配置的当前 Theme algorithm executable 分别生成五份 fresh golden，并分别运行旧字段比较：

```powershell
$validationPython = 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe'
& ./out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
& $validationPython openspec/changes/080-20261025-add-native-breadcrumb-and-steps/evidence/compare-baseline.py
& ./out/build/windows-msvc-headless/tests/portable/Release/rynui_portable_theme_algorithm.exe --write-goldens
& $validationPython openspec/changes/080-20261025-add-native-breadcrumb-and-steps/evidence/compare-baseline.py
git diff 5f71281a04c130949c33bc23506d263d7f6957c0 --exit-code -- design-tokens/ant-design/6.6.5/golden src/theme include/ryn/theme.hpp
& $validationPython scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff 5f71281a04c130949c33bc23506d263d7f6957c0 --check
```

结果：两次 fresh 比较均保留 d89eac7 的 **14,032** 个旧 typed leaf 值和 **28** 个 IIFE 原文/顺序。五份 golden 和 Theme 源码相对本轮基线完全无改动，包括已有 breadcrumb/steps 数据与 identity。文档示例编译/实际运行和 snippet 一致性均在两个配置的 affected 集合中通过。clang-format **22.1.3** 检查 **631** 个自有文件、0 failures；doctor healthy；strict **80/80**；diff check 通过。

## Task closure / Handoff

以上实际验证完成后才重新勾选 tasks 2.1/3.1/4.1，实施/共同验收恢复 **4/4**。保留了 R1 原生子控件和外扩焦点覆盖测试原文；没有引入生产诊断 API 或修改通用 registry/Input、Theme、Core/renderer ABI/backend/SDL 边界。

待父线程发起全新独立 **Astra xhigh R3**；本线程不宣称该审查已通过，父线程集成仍由父线程处理。没有 push、PR、archive、子线程、root checkout、Gallery 或全局文档改动。未执行的原生窗口/GPU、Linux、系统 IME、辅助技术不记为通过。
