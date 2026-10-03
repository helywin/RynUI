# 2.2 逐行视觉布局共同验收

2026-10-03，实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED。

Debug/Release affected CTest 各 8/8：bidi_layout、text_engine、text_state、text_scene_service、text_render_integration、glyph_scene、typography_component、typography_interaction；2.31s / 1.70s。消除 visual unit level 的 int→uint8 隐式转换 warning 后，重新构建并运行两配置 bidi_layout 均通过；既有 glyph scene fixture 的 uint32→float warning 保持基线。

新增测试直接核对 `A אבג 12 B` 的已知视觉 byte 顺序；混合 Hebrew/Arabic/数字/Latin 软折行及硬空行逐 glyph 唯一覆盖，逻辑 line byte 范围、逐行 L1 levels、visual x/width 一致。GlyphScene 使用同一 visual glyph 顺序，与实际 atlas entry/UV/physical phase/position 对应。

TextState 回归证实 width-only 更新复用分析 storage 且不 shape、direction 恰好一次 shape/frame invalidation、非法/equal direction 不修改状态，ellipsis display 前缀拥有独立分析/方向且 unchanged sync 不重新搜索或 shape。

clang-format 22.1.3：455 自有 source files，0 failures；doctor healthy、strict 55/55、diff check 通过。所有元数据位于 CPU measurement，GPU ABI 未更改。Caret/Input/不连续 coverage 与 native 证据由后续独立任务验收。
