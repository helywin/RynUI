# Astra R1 修复后的组合验收与 R2 交接

## Baseline / Scope

R1 修复提交：`19f0dcc`，基于本分支 `6dd6f415814293a830435c4a4f2481e42a22f98a`。原始基线仍为 `d89eac780eb4eb981813a5704dd51e7e79d4b2a9`；没有导入父 main 或其他 WIP。两项 P2 的修复前 Debug/Release 5/5 失败与修复后 focused 7/7 记录在 05-astra-r1-fixes.md。

实际环境：Windows / MSVC 19.51.36256 / Ninja Multi-Config / windows-msvc-headless，HEADLESS + RECORDING。所有 build 并发上限为 4，没有修改 Windows stack 或构建边界。

## Combined checks

以下命令在 x64 Visual Studio Developer Environment、UTF-8 控制台、`VSLANG=1033` 执行。实际 target / CTest 集合：

```powershell
$targets = @(
    'rynui_navigation_model_tests',
    'rynui_navigation_review_tests',
    'rynui_breadcrumb_component_tests',
    'rynui_breadcrumb_theme_tests',
    'rynui_steps_component_tests',
    'rynui_steps_theme_tests',
    'rynui_navigation_examples_tests',
    'rynui_collapse_component_tests',
    'rynui_collapse_theme_tests',
    'rynui_segmented_component_tests',
    'rynui_segmented_theme_tests',
    'rynui_feedback_component_tests',
    'rynui_feedback_theme_tests',
    'rynui_form_component_tests',
    'rynui_form_theme_tests',
    'rynui_date_picker_component_tests',
    'rynui_date_picker_theme_tests',
    'rynui_date_picker_review_tests',
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
    'rynui_portable_modal_theme',
    'rynui_portable_input_component',
    'rynui_portable_menu_component',
    'rynui_portable_dropdown_component',
    'rynui_portable_text_component',
    'rynui_portable_rounded_effect_scene',
    'rynui_portable_layout_engine',
    'rynui_portable_flex_component',
    'rynui_portable_scene_backend'
)
cmake --build --preset windows-msvc-headless-debug --target $targets --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui\.(navigation_model|navigation_review_[a-z_]+|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|navigation_examples|collapse_component|collapse_theme|segmented_component|segmented_theme|feedback_component|feedback_theme|form_component|form_theme|date_picker_component|date_picker_theme|date_picker_review_(wheel|metrics|inheritance|preflight)|navigation_docs_examples|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|modal_theme|input_component|menu_component|dropdown_component|text_component|rounded_effect_scene|layout_engine|flex_component|scene_backend|backend_boundaries))$' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target $targets --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui\.(navigation_model|navigation_review_[a-z_]+|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|navigation_examples|collapse_component|collapse_theme|segmented_component|segmented_theme|feedback_component|feedback_theme|form_component|form_theme|date_picker_component|date_picker_theme|date_picker_review_(wheel|metrics|inheritance|preflight)|navigation_docs_examples|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|modal_theme|input_component|menu_component|dropdown_component|text_component|rounded_effect_scene|layout_engine|flex_component|scene_backend|backend_boundaries))$' --output-on-failure
```

最终结果：Debug **46/46**，31.25 秒；Release **46/46**，17.83 秒。ignored 原始日志：`out/080-r1-combined-debug.log`、`out/080-r1-combined-release.log`。

除五个新 review cases 外，包含原 41 项共同回归：API/model/examples、两个组件/Theme、Feedback/Spin、Collapse、Segmented、Form、DatePicker 及四项 review、Theme algorithm/runtime/default/scope、Button、Focus order/state/lifecycle、Modal component/boundary/Theme、Input、Menu/Dropdown、Text、rounded scene、Layout/Flex、Recording scene backend 与 backend boundaries。没有用此结果代替真实平台验收。

新 cases 同时执行真实子控件 hit/focus/commit/activation、current/display 往返、disabled/hidden/window/Modal/Spin 的屏蔽/恢复，以及单/多项、H/V、短/零尺寸的实际 outline 四边 coverage、外部 ancestor/document clip、严格 glyph/surface/hit clip 与子 Button shadow。原组件测试继续验证 controlled 拒绝/重试、键盘/指针导航、身份、生命周期/回调销毁、隐藏流重排与空闲帧。

## Fresh goldens / Docs / Checks

Debug 和 Release 当前构建的 Theme algorithm executable 分别执行 `--write-goldens` 后均比较：

```powershell
& ./out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
& ./out/build/windows-msvc-headless/tests/portable/Release/rynui_portable_theme_algorithm.exe --write-goldens
$validationPython = 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe'
$formatter = 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& $validationPython openspec/changes/080-20261025-add-native-breadcrumb-and-steps/evidence/compare-baseline.py
& $validationPython tests/verify_navigation_examples.py
& $validationPython scripts/format-code.py --check --clang-format $formatter
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff 6dd6f415814293a830435c4a4f2481e42a22f98a --check
```

五份 fresh golden 共 **14,032** 个旧叶字段类型和值与 d89eac7 保持，原 **28** 个 IIFE 原文及顺序保持；R1 修复没有修改任何 Theme 数据或 golden 内容。文档 C++ snippets 与实际编译执行的样例一致。clang-format **22.1.3** 检查 **630** 个自有文件，0 failures；doctor healthy；strict **80/80**；diff check 通过。

## Handoff

只有全部上述实际验证通过后才重新勾选 080 tasks 2.1/3.1/4.1。实施/共同验收 **4/4**；交父线程安排全新独立 **Astra xhigh R2**，本线程不宣称 R2 已通过。

没有 push、PR、archive、子线程或父 checkout 改动；未修改 Gallery/support overlay/README、Date/Form 通用 Input reset hooks、Core/renderer ABI/backend/SDL 边界。Gallery、原生窗口/GPU、Linux、系统 IME、辅助技术未执行，不记为通过；composition 仅为 headless 注入事件。
