# 平台通用组合验收与审查交接

实际环境：Windows / MSVC 19.51.36256 / Ninja Multi-Config / `windows-msvc-headless`，HEADLESS + RECORDING。所有 build 使用 `--parallel 4`，没有调整链接 stack。此 worktree 从 `d89eac780eb4eb981813a5704dd51e7e79d4b2a9` 开始，不合并其他 WIP。

## 可重放命令

先按照 docs/development/building.md 进入 x64 Visual Studio Developer Environment，保持 UTF-8 控制台与 `VSLANG=1033`。本次完整 target 集合：

```powershell
$targets = @(
    'rynui_navigation_model_tests',
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
ctest --preset windows-msvc-headless-debug -R '^rynui\.(navigation_model|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|navigation_examples|collapse_component|collapse_theme|segmented_component|segmented_theme|feedback_component|feedback_theme|form_component|form_theme|date_picker_component|date_picker_theme|date_picker_review_(wheel|metrics|inheritance|preflight)|navigation_docs_examples|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|modal_theme|input_component|menu_component|dropdown_component|text_component|rounded_effect_scene|layout_engine|flex_component|scene_backend|backend_boundaries))$' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target $targets --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui\.(navigation_model|breadcrumb_component|breadcrumb_theme|steps_component|steps_theme|navigation_examples|collapse_component|collapse_theme|segmented_component|segmented_theme|feedback_component|feedback_theme|form_component|form_theme|date_picker_component|date_picker_theme|date_picker_review_(wheel|metrics|inheritance|preflight)|navigation_docs_examples|portable\.(theme_algorithm|theme_runtime|default_theme|theme_scope_component|button_component|focus_order|focus_state|focus_lifecycle|modal_component|modal_boundary|modal_theme|input_component|menu_component|dropdown_component|text_component|rounded_effect_scene|layout_engine|flex_component|scene_backend|backend_boundaries))$' --output-on-failure
```

最终结果：Debug **41/41**，34.27 秒；Release **41/41**，16.71 秒。

覆盖新导航 model/component/Theme、两份文档示例编译并真实挂载激活、文档片段一致性、Collapse/Segmented/Feedback（Spin）/Form/DatePicker、四项 DatePicker review 回归、Menu/Dropdown、默认 Theme/ThemeScope/runtime/algorithm、Button、Focus order/state/lifecycle、Modal component/boundary/Theme、Input、Text、rounded scene、Layout/Flex，以及实际 Recording scene backend 与 backend boundaries。既有 popup wheel 路由顺序未改动。

`navigation_examples_tests.cpp` 编译 docs/breadcrumb.md 和 docs/steps.md 的完整 C++ 函数示例，并实际运行 pointer 激活和 reactive 更新；`verify_navigation_examples.py` 防止文档与编译样例漂移。空 items 的两个 host 在带 gap 的 Flex 中不留下流占位或 interaction。

## Button 测试栈修正

初次 Debug 组合结果为 40/41，Button 异常退出 `0xC00000FD`。用 Windows Kits x64 cdb 实查：

```text
cdb.exe -o -c "sxe sov;g;kv;q" out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_button_component.exe
__chkstk
resolve_theme lambda_1
resolve_theme
ThemeScope::ThemeScope
ThemeScope::create_default
ComponentHost::ComponentHost
...
test_reactive_state_matrix_and_minimal_dirty_ranges
main
```

该旧测试函数中的多次完整默认 ThemeSnapshot 临时对象使 caller frame 为 `0xD8B10`（887,568 bytes，约 867 KiB），构造 Fixture 时再解析默认 Theme 导致默认 Windows stack 溢出。Button 的默认值断言改用已有 `detail::default_theme_snapshot()` 不可变引用，保留全部断言和显式 config 解析；`portable.default_theme` 仍验证缓存快照等于 fresh `resolve_theme()`。未增大 stack，未改变生产 Theme 的 static default config、heap Prop/binding/preflight/self-reference 保护，未改原 28 个 IIFE。针对性 Debug Button 1/1 通过后，重新执行最终 Debug/Release 41/41。

## Golden 与边界

```text
out/build/windows-msvc-headless/tests/portable/Release/rynui_portable_theme_algorithm.exe --write-goldens
python openspec/changes/080-20261025-add-native-breadcrumb-and-steps/evidence/compare-baseline.py
python tests/verify_navigation_examples.py
python scripts/format-code.py --check --clang-format <clang-format-22.1.3>
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

五份 fresh golden：`compact-dark` 2,807、`compact` 2,806、`dark-compact` 2,807、`dark` 2,806、`default` 2,806，共 **14,032** 个旧叶字段类型和值保持。只有 identity 与新增 breadcrumb/steps 域变化；原 **28** 个 IIFE 原文/顺序保持。clang-format 22.1.3 检查 **629** 个自有文件、0 failures；doctor healthy；strict **80/80**；diff check 通过。

两个组件的空闲无持续帧/deadline、retained 身份、真实 glyph/icon layers/surface/hit clip、键盘/指针/受控请求、原生子控件、输入边界及销毁已在前两个阶段的 component tests 中验证。实际 headless 构建包含共同 renderer 与 Recording backend；没有更改 Core/renderer ABI/backend/SDL 公开边界。

## 交接状态

实施与平台通用验收完成，交父线程安排全新独立 Astra xhigh R1；本线程没有创建审查子线程，也没有宣称独立审查已通过。没有 push、PR、archive，未修改 Gallery/support overlay/README、079 或父 checkout。Gallery、原生窗口/GPU、Linux、系统 IME 与辅助技术均未验收，不记为通过。
