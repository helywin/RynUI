# 平台通用：Input 键盘编辑

2026-09-12；任务 7.4；Windows/MSVC `windows-msvc-debug`。

## 实现

- FocusManager 的 owner-scoped text-edit handler 在 Tab traversal/Button activation 前处理命令；handler 使用 retained shared ownership，回调销毁后重新检查 generation/focus。
- 平台中立 Key 增加 Left/Right、Home/End、Backspace/Delete、Escape、A/C/X/V/Z/Y；SDL 3.4.14 adapter 保留 key/action/modifiers/repeat，并提供平台 primary modifier。Windows/Linux 是 Control；Command 仅有归一化分支与 fake-event 合同，不声称 macOS 原生验收。
- grapheme navigation 和 Shift selection、剪贴板、undo/redo 使用既有 editor/runtime。支持 primary+Shift+Z redo。重复 navigation/deletion 生效；重复 clipboard/history/select-all shortcut 不重放，Enter 仅 non-repeat key-down submit。
- 字符输入仅通过 TextCommitted，Key A/Space 不插入字符。Ctrl+Alt（AltGr）或另一平台的非 primary modifier 不误触编辑 shortcut。
- composition 期间 Input 优先消费按键，保留 committed value/selection；Escape non-repeat down 取消 composition。无 composition 的 Tab/Shift+Tab继续焦点遍历。
- read-only 允许导航、select-all、copy，不允许 mutation/submit。disabled 不接收编辑命令。
- clipboard/onChange/onSubmit 可销毁 owner；onChange 可同步改变 disabled/read-only。eligibility synchronize 在外层 Focus dispatch 收口，普通输入 dispatch 重入仍拒绝；禁用后重启 Props 不恢复已失焦的 native session。

## 验证

Input keyboard/pointer/component、platform input/SDL adapter、Focus order/state/lifecycle/allocation、Button keyboard activation、clipboard/history/reconcile 14/14 通过（6.76 秒）。随后补充 callback eligibility 测试，Input keyboard 再次通过（0.74 秒），Input component/pointer/GPU 也通过。

20,000 次 Shift+Home/End repeat 经实际 FocusManager 和场景同步，预热后 0 C++ heap allocation。测试覆盖 CJK/emoji grapheme、controlled echo、read-only/disabled、clipboard failure、三类同步自销毁、IME 优先级、modifier-shaped fake events、key-up/repeat 不重复 submit、Tab traversal。

前一指针阶段全量 CTest 198/198 已通过（277.44 秒）。随后本键盘阶段完整 CTest 199/199 通过（235.99 秒），其中 256 Input 基准通过（115.65 秒）；此回归不包含后续 caret blink 实现。上述不替代原生 Windows/Linux IME、clipboard、window 或 GPU 验收。
