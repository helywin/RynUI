# main 集成与主题栈修复（2026-10-24）

- main 基线 `810b161c2c5fb68dc0eefd89b82225437896a8f8`，合并 Segmented `d5d5ead34e9178a5d7189bd249085df34777eb2a`。组件 R1/R2 的宽度/居中与 glyph 裁剪修复已通过独立 Astra R3，无 P1/P2。
- 保留 Collapse、Table、Menu/Dropdown、InputNumber 及全部既有 Theme 字段、订阅、identity/diagnostics、equality 与 host 相对顺序。新构建 Theme algorithm 生成五份 golden；除 identity 与新增 segmented 外，全部既有字段值保持一致。
- 第一次共同 Debug 为 9/10：既有 Switch features 退出 `0xC00000FD`。CDB 确认 `Signal<ThemeConfig>::set → Theme binding bridge → ThemeScope::update → resolve_theme` 发生 stack overflow；不是 Segmented 输入或绘制失败。
- `resolve_theme` 原 Debug 调用帧约 253 KiB，各组件 algorithm 的 seed/map/alias 临时值在同一函数帧中累计。现在把 24 个组件的既有解析块放入立即调用的引用捕获 lambda，保持顺序、继承、override 和返回值，逐组件完成临时计算后再组装 snapshot。CDB prologue 栈分配为 `0x127B8`，约 74 KiB。未修改线程栈、通用 Prop/Binding API 或 Switch 测试。
- 原 Switch/Theme algorithm 2/2 恢复通过；重新链接受影响 targets 后，Windows MSVC `windows-msvc-headless` Debug **10/10**（9.62 秒）、Release **10/10**（5.16 秒）。范围：Segmented 三项、Collapse、Table、Dropdown、Select、Switch features 和 Theme algorithm/runtime。
- clang-format 575 文件，0 failures；doctor healthy；strict validate **73/73**（工作树同时有 074 Gallery 规划）；diff check 通过。

原始日志位于 ignored `out/segmented-switch-stack.log`、`out/segmented-resolve-frame.log`、`out/segmented-stack-fix-*.log` 与 `out/segmented-root-*.log`。本集成没有运行 Segmented 原生窗口/GPU/Linux/系统 IME 验收；新的共享主题调用分离另交 Astra 复核。
