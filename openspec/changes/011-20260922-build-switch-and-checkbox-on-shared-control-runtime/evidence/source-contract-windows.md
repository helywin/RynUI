# 来源与公开合同验收（Windows）

- 平台：Windows 10.0.26200，x64；MSVC 14.51.36231；正式 `windows-msvc` Ninja Multi-Config preset，Debug。
- 来源：Ant Design 6.6.5，commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；四份关键 doc/style 文件 SHA256、Props、状态矩阵、Switch middle/small、Checkbox 固定尺寸及 Token identity 固定于 `source-contract.json`。`tools/verify_selection_controls_source.py` 离线核对 manifest、change 012 diff 与现有本地来源。
- 公开类型：`SwitchSize` 仅含 Middle/Small；Checkbox 无 size 入口。两者只有 typed `Prop<bool>` 状态、`onChange(bool)` 与外部 `LayoutStyle`；Checkbox label 为独立 typed slot。受控 checked 与 defaultChecked 冲突在 mount 阶段拒绝，交互实现在后续阶段接入。
- 命令：`./scripts/build-windows.ps1 -Configuration Debug -SkipTests` 通过；`ctest --test-dir out/build/windows-msvc -C Debug -R "rynui.selection_controls_(source_contract|public_api)" --output-on-failure` 为 2/2 通过；`git diff --check` 通过。
