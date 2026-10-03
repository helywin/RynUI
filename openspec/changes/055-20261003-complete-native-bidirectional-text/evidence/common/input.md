# 3.2 输入与选区共同验收

2026-10-03，实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED。

Debug/Release affected CTest 各 22/22，218.70s / 14.93s。新增 bidi_input 与既有 Input、Password、TextArea、OTP、Typography、TextScene/GlyphScene、session/clipboard/display、caret blink/GPU 和 input_scene_allocation 回归通过。

真实 FontRuntime/组件/编辑器/会话/剪贴板端口 fixture 验证 Hebrew RTL Left/Home/End/Shift、逻辑 copy/删除/undo、Arabic preedit/提交及 disposal/stale stamp，混合范围的两段视觉间隙及 selected glyph logical ownership，同一行 run affinity 指针命中、受控更新保留 owner/ref、RTL 软折行/vertical scroll/IME area，以及 Password emoji grapheme 显示到 committed byte 映射。OTP 原有组方向及 sensitive display 回归保留。

256 个 Input，selection 和 composition-selection 各 20,000 updates，Debug/Release allocations=0，dispatch/sync allocations=0；不重 shape/measure/layout 无关组件，不上传 atlas/effects，retained capacity/identity 不变。最初 content_range 的每帧命令发布导致分配，已改为仅范围发生改变时发布；单行最低 1 selection/2 overlay 槽位保持既有局部上传合同。新增分段 clip 使用 CPU ownership，不修改 GPU ABI。

clang-format 22.1.3：457 自有 source files；format check、doctor healthy、strict 55/55、diff check 通过。这里验证输入端口与逻辑/场景合同，Windows 真实窗口和 OS 候选 UI 不由此证据替代。
