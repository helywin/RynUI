# TextArea 多行交互验收

日期：2026-10-03。实际环境：Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config。

## 实现合同

- 二维命中/拖选按实际测量行定位 grapheme byte；跨硬换行包含 LF，空行保留 selection coverage。
- Home/End 使用视觉行，primary Home/End 使用文档；上下/Page 保留期望 x，Shift 保留 anchor；软折行共享 byte 保存 upstream/downstream affinity。
- Plain Enter 形成独立 LF 撤销事务，primary Enter 提交；活跃 IME 拥有导航/Enter/快捷键。多行 preedit 按行生成下划线，committed 值/统计不包含候选。
- wheel 保留用户滚动位置；下一次编辑/导航露出 caret。physical pixel 对齐的边界返回未消费，包含分数缩放回归；scroll/selection 不 shape/raster。
- resize 使用 primary PointerRouter capture；secondary release 不取消 primary capture。配置/autoSize/disabled/卸载取消资格。首次与变化后的边框逻辑尺寸回调使用副本及存活身份检查。
- glyph primitive 的 line_ranges 仅为 CPU 元数据；共享 selected text view 以每行 logical clip 修改 retained geometry，GPU packed ABI 未变。重新插入前序 text range 后行 metadata 同步 remap。

## 验证

19 个 affected CTest：text_area、selection_component、input_scene_allocation、focus_order/state/lifecycle、interaction_registry、pointer_route、text_scene_service、typography_component/interaction、input_component、password_component、search_component、glyph_scene、glyph_gpu_resources、logical_scene_packing、input_gpu、text_render_integration。

- Debug：19/19，104.43 秒，包含 96.68 秒 Input scene allocation benchmark。
- Release：19/19，11.37 秒，包含 5.92 秒 Input scene allocation benchmark。
- 最终 TextArea Debug/Release 单独复核：各 1/1，包含 per-row IME underline、跨行剪贴板、Enter history、软折行 affinity、Page/期望 x、fractional scroll boundary、secondary release 与 callback 卸载。
- `tests/text_scene_service_tests.cpp` 覆盖空行 metadata、不同 per-line clip、equal noop、scroll、前序 range 扩展后的 remap、恢复 common clip，且 shape/raster 计数不变。

构建/CTest 日志：`out/053-interaction-debug.log`、`out/053-interaction-release.log`、`out/053-interaction-area-final.log`。共同逻辑证据不代表真实 OS IME 候选窗口、GPU 或 Linux 验收；Gallery/Windows 留给后续独立阶段。
