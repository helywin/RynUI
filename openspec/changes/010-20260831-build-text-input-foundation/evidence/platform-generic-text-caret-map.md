# Unicode 与字形光标映射验证

2026-09-12，Windows / MSVC x64，正式 `windows-msvc-debug`，Ninja Multi-Config。

- TextEngine 新增 `map_carets`：核对 shaped scalar 与原始 UTF-8 source 一致，由 utf8proc 计算合法 grapheme boundary，再由 HarfBuzz glyph cluster/advance 构造独立 `TextCaretMap`。
- 每个 stop 保存 byte、x、baseline 与下一 grapheme 覆盖的 glyph range；查询要求匹配 revision。glyph cluster 不替代 Unicode 编辑边界；cluster 内多个 grapheme 按 advance 等分，跨 fallback cluster 的同一 grapheme 仍不拆分。
- nearest lookup 对重复 x 和等距离采用最早 logical boundary。空值、无宽字符、越界位置、NaN、过期 revision、无效 cluster、负 advance 和不匹配 source 均有确定行为。当前明确拒绝 RTL/multiline map，不宣称实现复杂 bidi visual navigation。
- 使用锁定 Noto Sans / Noto Sans CJK 的真实 FreeType/HarfBuzz shaping，覆盖 `ffi` ligature、combining、CJK/Latin fallback、emoji ZWJ、missing glyph 和 empty；直接验证 caret end 等于实际测量宽度。
- Debug build 与 TextCaretMap、TextEngine、TextState、Input、font dependency 5/5 CTest 通过。
- 20,000 次 lookup 循环（每次 nearest + exact），0 heap allocations。11 个 map preparation 分配失败点保持已发布 revision/stops 不变。
- Context7 官方 HarfBuzz 文档和锁定 14.3.1 的 `hb-buffer.h` cluster 注释共同核对：cluster 可合并且不等同于 Unicode grapheme。未新增或升级依赖。

本证据仅完成 6.1，不代表 Input retained scene、GPU、pointer journey 或原生 IME 验收通过。
