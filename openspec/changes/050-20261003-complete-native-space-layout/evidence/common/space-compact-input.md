# Compact 编辑控件阶段（平台通用）

实际环境：Windows/MSVC，`windows-msvc-headless`，2026-10-03。

Input/Password 接入最近 Compact 尺寸和外角，显式 Middle 优先；Search 在组合内挂载嵌套 Compact。共享 Flex 分配规则保留 basis、grow/shrink、min/max 冻结；复用同一 LayoutEngine generation。控件拥有的 prefix/suffix/Button slots 不继承外层组合。Input 按四角更新共同 logical effects，不改变 editor、IME session、interaction 或 slot 身份。

失活分支不接受 focus/Tab 或旧 pointer hit snapshot；Tooltip 关闭时取消失活分支的 focus/IME/capture。测试实际挂载、开启和关闭富内容编辑浮层，并拒绝关闭后的旧 session commit。

## 验证

- HEADLESS Debug：14/14，通过，5.35 秒。
- HEADLESS Release：14/14，通过，3.77 秒。
- 范围：input_component、password_component、search_component、search_public_api、input_public_api、space_compact、focus_order、focus_lifecycle、pointer_route、layout_engine、layout_allocation、flex_component、flex_features、tooltip_component。
- Compact 测试包含三尺寸继承/显式优先、RTL 更新不重建 editor/IME/slot/effect、中文 commit、Search 连续单边连接与 block/grow 宽度、popup 关闭清理和完整销毁。
- `python scripts/format-code.py --check --clang-format <VS clang-format>`：22.1.3，415 个自有 source，0 failures。
- `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check` 通过。

复现：分别通过 `windows-msvc-headless-debug/release` 构建上述 `rynui_portable_<name>`，然后对应 CTest preset 按这些名称运行。使用 VS Developer Environment 和 UTF-8 控制台。

这是 4.1 的可独立验证阶段；RadioButton/Addon 未完成，4.1 仍未勾选。本文件记录平台通用模拟编辑合同；Windows 真窗口/GPU 和真实系统输入证据属于独立平台任务。
