# 3.1 光标与覆盖共同验收

2026-10-03，实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED。

Debug/Release affected CTest 各 6/6：bidi_caret、bidi_layout、text_caret_map、input_display、text_editor、text_engine；0.68s / 0.41s。

新增 bidi 查询 20,000 cycles，allocations=0；Release allocation fault 准备失败 41 个位置均保留既有 coverage/map。既有 CaretMap 20,000 lookup cycles allocations=0，InputDisplay 20,000 composition cycles allocations=0（37 个失败位置），其余生命周期/selection/Unicode/editor 合同通过。

覆盖真实 Hebrew RTL、RTL ligature、混合 run boundary 双 affinity、视觉 adjacent/edges、两片段选择间隙、逻辑 byte 查询、非法 byte/revision、重复 glyph order/NaN metadata 拒绝、soft-wrap affinity 与 RTL 跨行箭头、2D clamp/preferred x、零宽 glyph tie，以及 fallback/control 多 cluster emoji grapheme 的紧急折行原子性。

初次单行 preparation 在 MSVC Debug allocation fault 下终止，原因是新增临时 STL 容器默认/move noexcept 的 iterator proxy 分配。共用 geometry 改用既有 scratch 和可抛异常构造后既有/新增故障回归均通过；测试 Debug CRT 诊断改为 stderr，关闭 abort/WER UI，临时阶段打印已移除。

clang-format 22.1.3：456 自有 source files，0 failures；doctor healthy、strict 55/55、diff check 通过。InputDisplay 的既有 int→optional<float> warning 将在下一输入接入阶段随相关函数整理。Input selected scene/IME/display affinity 与真实 native window 尚属后续任务。
