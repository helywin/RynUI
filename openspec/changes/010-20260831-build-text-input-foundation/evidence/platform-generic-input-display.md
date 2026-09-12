# Composition display 与 caret scroll 验证

2026-09-12，Windows / MSVC x64，正式 `windows-msvc-debug`。

- 独立 `InputDisplayState` 原子准备 committed prefix + transient preedit + committed suffix，维护显示 selection/caret/underline、placeholder visibility 与双向 byte mapping；collapsed replacement 支持明确 leading/trailing affinity。
- preedit scalar range 转为 UTF-8 byte 后再 clamp 到完整 display grapheme。未知范围使用 preedit 末端；组合字符内部的零长度范围不会产生非法 caret 或 selection。
- 显示内容与几何 metadata 分开修订；composition range 相同文本更新不增加 display revision。Input 使用所属 generation 的 TextCaretMap 缓存，TextState 内容/字体/布局 revision 变化才重建映射；material 变化不重建。
- Input host 已接入 composition 显示。预编辑不修改 committed value/onChange，不引发外部 Measure/Layout；仅 range 变化不 shape。commit/controlled echo 与当前显示一致时不重复 shape，同时 committed value 的必要 intrinsic layout 仍可失效。
- caret-visible scroll 包含 caret 宽度，支持 End/Home、零宽 viewport、viewport 扩大、value shrink、placeholder 和 stale-map 拒绝。
- direct Input headless journey 验证中文预编辑长文本、placeholder 隐藏、候选选区、End 可见、Home/End scroll、一次 commit/onChange、同步 echo、external shorter value 与 empty commit cancellation；root measure count 和 shape count 按预期保持稳定。
- Debug build 与 TextCaretMap、TextState、InputDisplay、InputComponent、TextSceneService 共 5/5 CTest 通过。
- InputDisplay：20,000 composition-range cycles，0 allocations；37 个 allocation failure points 保持已发布 display/revision 不变。

本证据只完成 6.2。固定绘制层、selected glyph material、GPU clipping/alignment 与真实窗口仍在后续任务；没有声明原生 IME 或视觉已验收。
