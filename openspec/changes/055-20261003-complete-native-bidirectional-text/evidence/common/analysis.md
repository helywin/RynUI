# 1.1 段落分析共同验收

2026-10-03，实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED。

- Debug `rynui.bidi_analysis`：91,707 BidiCharacterTest + 770,241 BidiTest cases，22.15s；Release 相同全量用例，1.23s。
- Debug/Release `rynui.bidi_dependency`：有效 SYSTEM、测试关闭、精确版本拒绝、缺 target、缺 fixture、错误 fixture hash、非法 mode 均通过。
- Debug/Release dependency lock、实际 headless/Core include/link/编译图守卫通过；实际 SheenBidi MSVC 编译使用 `-std:c17 /experimental:c11atomics`。
- clang-format 22.1.3：453 自有 source files，0 failures；OpenSpec doctor healthy，strict 55/55，diff check 通过。

初次 UTF-8 分析在 BidiTest line 208322 (`S RLE PDI R`) 暴露 code-unit L1 重置切开多字节 scalar。包装改为 UTF-32 scalar 分析、UTF-8 byte offset 回映射后两个全量 corpus 均通过，未修改上游库。SYSTEM 版本拒绝测试的诊断匹配和编译图守卫对 MSVC `-std:c17` 拼写的识别已修正并复测。

来源固定 SheenBidi 3.0.0，archive SHA256 `86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786`。Unicode 17 两份 corpus hash、source URL、license 见中央 dependency lock，测试读取构建树 fixture。

此阶段只验收 Core 分析；shaping、视觉布局、编辑与 native window 在后续独立任务验收。Linux 原生项保持未验收。
