# Astra R1 修复证据

## Form 字体可表示边界

基线 `1276bbf` 上先加入回归，再执行以下命令（独立官方 MSVC headless 构建树、最多 4 jobs）：

```powershell
& .\out\form-build.ps1 -Targets rynui_form_theme_tests,rynui_form_component_tests -Filter '^rynui.form_(theme|component)$'
```

Debug 结果为 0/2；实际失败消息分别是 `Form resolver accepted a font that rounds to zero logical pixels` 与 `invalid Form font partially published Theme or mutated actual Text/input state`。

修复在 `apply_form_override` 结束处检查最终派生的 `font_size >= 0.5` 且有限，包含直接 override、继承与 component algorithm 的统一出口。Text 先对 logical font size 做 `lround`，再交给 DPI resolver，因此不能用放大后的物理像素掩盖零 logical font。正常指标保持原值。

同一命令 Debug 2/2；加 `-Configuration Release` 后 Release 2/2。新增测试覆盖初始失败清理、响应更新拒绝时 Theme generation/snapshot 与实际 Text typography/scene revisions/geometry/input session 不变、极小正数/阈值相邻值/NaN/infinity、component seed 派生、恢复、真实 glyph 与 1x/2x DPI。此处是 headless font/input 合同，不代表 native GPU 或系统 IME 验收。

`out/form-build.ps1` 仅为本地命令包装：初始化 VS 2026 MSVC 环境，然后运行 `cmake --build --preset windows-msvc-headless-{debug,release} --parallel 4 --target ...` 和 `ctest --preset windows-msvc-headless-{debug,release} -R ... --output-on-failure`；输出全部属于本 worktree。

## 真实 ref 目标的失败焦点

在 `9c92051` 加入 400dp Form、80dp 首个 required Input、全宽第二个 required Input、document clip `{100,0,300,900}` 回归，先运行：

```powershell
& .\out\form-build.ps1 -Targets rynui_form_component_tests -Filter '^rynui.form_component$'
```

Debug 0/1，实际失败 `fully clipped actual Input stole focus through its visible wrapper`。修复为内部 `FormFocusContext` 在 ref 的捕获代际检查之后，由 Input/Checkbox 所属 host 对真实目标 node 检查同一 WindowComponentServices、branch/input boundary、bounds 与祖先/document clip，再调用原有 focus。普通 ref 的公开逻辑 focus 语义未改变；自定义 `.focus(...)` 的真实目标资格仍由应用负责。

新回归同时检查部分可见仍可聚焦、零高跳过与恢复、实际祖先 clip、隐藏的真实 Input、Spin 阻止的真实目标、删除 target 后后续无效字段、retained editor/slot identity、跨窗口 ref，以及跳过裁剪目标时可见 Input 的 preedit/session 不变。Checkbox 独立检查窄实际目标的全裁剪/部分可见；既有重绑和回调销毁用例继续执行。Modal 用例使用 `{0,0,1,1}` document clip，断言窗口层的真实 Input 仍可聚焦而背景 Form 被排除。

修复后的 Debug **13/13**、Release **13/13**（Form 4 项、InputNumber、Feedback、portable Theme algorithm/runtime、Selection、Modal、focus lifecycle、TextArea、Input）。完整 targets 和 preset/filter 与 [platform-generic.md 的定向构建](platform-generic.md#定向构建与测试) 相同；通过包装命令显式传入那 13 个 targets 和相同 `-Filter`，Release 额外传 `-Configuration Release`。五份金样相对 `1276bbf` 无改动，相对 `4efa310a256019192e01f0738ee226d16ad44396` 的语义比较仍为 5/5、26 个既有引用 IIFE 原文及顺序保留。合并父基线后的最终结果在后续段记录。

## 合并父线程已验证基线

字体修复提交 `9c92051`、真实目标焦点修复提交 `99bb6c4`；确认 `git status --porcelain` 为空后执行：

```text
git merge --no-ff --no-commit 6f1b38a53486907bfc890939335180e8ac2df4b7
```

自动合并无冲突。未手工改动父线程 Gallery/support/completion/073/076/077 文件；077 仅随父提交导入 planning，不包含 DatePicker implementation。`git diff 6f1b38a -- src/theme/theme_runtime.cpp src/theme/theme_runtime.hpp` 仅显示 Form identity/accessor/dirty domain 接线；父线程的 `validate_children`、`resolve_validation_snapshot`、update/commit_snapshot、notify/recompute 原文不变，Theme binding host 与 Empty/Result/Popconfirm/Table host 相对父基线 diff 为零。

组合验收使用 [platform-generic.md 的最终 20 个 targets 与完整 CTest filter](platform-generic.md#父基线合并与最终组合验收)，两次实际包装命令分别传全部 20 个 targets：

```powershell
$targets = @(
    'rynui_form_model_tests', 'rynui_form_public_api_tests', 'rynui_form_component_tests', 'rynui_form_theme_tests',
    'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime', 'rynui_portable_selection_component',
    'rynui_portable_modal_component', 'rynui_portable_focus_lifecycle', 'rynui_portable_text_area',
    'rynui_portable_input_component', 'rynui_input_number_component_tests', 'rynui_feedback_component_tests',
    'rynui_empty_result_public_api_tests', 'rynui_empty_result_theme_tests', 'rynui_empty_result_component_tests',
    'rynui_table_component_tests', 'rynui_portable_dropdown_component', 'rynui_portable_popconfirm_component',
    'rynui_portable_select_component'
)
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target @targets
ctest --preset windows-msvc-headless-debug --output-on-failure -R "^rynui.(form_.*|empty_result_.*|table_component|input_number_component|feedback_component|portable.(theme_algorithm|theme_runtime|selection_component|modal_component|focus_lifecycle|text_area|input_component|dropdown_component|popconfirm_component|select_component))$"
cmake --build --preset windows-msvc-headless-release --parallel 4 --target @targets
ctest --preset windows-msvc-headless-release --output-on-failure -R "^rynui.(form_.*|empty_result_.*|table_component|input_number_component|feedback_component|portable.(theme_algorithm|theme_runtime|selection_component|modal_component|focus_lifecycle|text_area|input_component|dropdown_component|popconfirm_component|select_component))$"
```

Debug **20/20，32.24 秒**；Release **20/20，19.91 秒**，包含父线程新加入的 descendant prepublication rollback/recovery regressions 与本次 Form 原生控件回归。全部使用本 worktree 的独立输出、MSVC、Ninja Multi-Config、最多 4 jobs。

实际执行本次新构建的 `out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens`，再执行 `python out/verify-form-goldens.py 6f1b38a53486907bfc890939335180e8ac2df4b7`，结果为五份既有数据类型/值逐项相等（除 identity/new Form keys）及 **26 个既有引用 IIFE** 原文和顺序保留。`git diff --exit-code 1276bbf -- design-tokens/ant-design/6.6.5/golden` 退出码 0，证明重新生成后五份金样仍逐字节不变。

从当前 `docs/form.md` 提取 cpp fenced block 后运行 `cl /nologo /std:c++20 /permissive- /Zc:__cplusplus /utf-8 /EHsc /Zs /Iinclude out/form-doc-example.cpp`，退出码 0。`python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"` 使用 22.1.3 检查 **596** 个自有源，0 failures；doctor healthy、strict all **77/77**，working/staged diff check 通过。

075 实施 tasks 共 10/10 完成；此记录不代表 Astra R2 已审查。未运行全套 CTest、真实 GPU/Linux/系统 IME/OS DPI/人工视觉验收；未 push、创建 PR、archive 或 children，交父线程继续审查和合并。
