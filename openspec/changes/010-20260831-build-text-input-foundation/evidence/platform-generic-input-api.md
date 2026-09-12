# Input API、生命周期与布局验证

2026-09-12，Windows 11 / MSVC x64，正式 `windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config。

## 已实现合同

- `ryn::Input`、typed `InputProps`、独立 prefix/suffix slot；`ControlSize` 移至公共头，Button 源兼容。公开头不引入 SDL、renderer 或内部 identity。正向 standalone header 编译、链接入口和六项负向编译合同覆盖 narrow value/callback、foreign slot、renderer、Modifier、SDL。
- value/defaultValue 冲突在取得 component/editor/interaction/scene identity 前拒绝；同一 generation 固定 controlled mode。非受控值归 editor 持有；受控同步 echo 保留 selection/history，不重新执行 slots。
- Input host 共享现有 ComponentHost、FocusManager 与 PointerRouter，持有独立 editor/session/clipboard command service；owner generation、mount-time slot reuse、throwing slot rollback、callback 自销毁均通过。
- capture、focus、composition、native session 和 owner-scoped caret deadline slot 随禁用/只读/失焦/销毁清理。未聚焦的只读 Input 恢复 editable 不会启动 native session。deadline slot 已实现；自动 blink policy、ticking 和 frame-loop 接入仍属于 7.5，未宣称 blink 已完成。
- 三个独立内部节点承载 prefix、editable viewport、suffix。affix 优先测量，editable 获取剩余宽度；极窄宽度可收缩为零且不越过 root。使用真实 FreeType/HarfBuzz Noto Sans + CJK 字体测量，单行不换行，保留 baseline、viewport clip、bounded horizontal scroll offset，并在文本缩短时 clamp。
- value/placeholder/font/size 的必要文本和布局失效；隐藏 placeholder 不触发布局，slot 内容变化不重新 shape editable value。status/disabled/readOnly 不触发 Shape/Measure/Layout。nested Theme 与 Signal batch 保持 slot/sibling 执行次数和无关文本 shape count；全局 lineWidth 使用独立 token identity，不读取 Button 的组件 override 作为 Input 样式。

## 测试结果

- 正式 Debug build 通过；全量 CTest 191/191 通过。
- 后续 deadline 生命周期补充：受影响 API、Input/Button、Theme、Layout、依赖合同 9/9 通过。
- 最终 slot 更新补充：`rynui.input_component` 通过，直接执行输出 `Input layout simulated-scale cases=768`。
- 矩阵为 3 sizes × 4 slot combinations × 4 text values × 4 logical widths × 4 simulated scales；额外覆盖 empty placeholder、min/max 与 parent Constraints、outer clip、长文本 scroll clamp、真实 pixel-size font resolution。
- Focus/capture/composition/deadline teardown、controlled callback 自销毁、submit callback 自销毁、readOnly blur、generation reuse 与 slot exception rollback 通过。
- OpenSpec doctor、strict validate 与 `git diff --check` 为提交门禁。

## 边界

此阶段提供 API、生命周期与布局基础，不代表 editable glyph/selection/caret/composition retained scene 已完成。TextCaretMap、caret-visible scrolling 和固定绘制层属于阶段 6；Input Token 完整视觉、pointer/keyboard commands 与 blink 属于阶段 7。没有提高 Gallery Input support status，没有更新 Windows/native Linux 验收 checkbox。四档 simulated scale 不是原生 DPI、GPU 或人工 IME 验收。
