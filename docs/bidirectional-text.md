# 双向文本

055 分阶段补齐原生桌面双向文本。段落分析、逻辑 shaping 和逐行视觉布局已接入；光标、选择及公开组件接入仍按 change 的 tasks 实施，不能将布局测试当成输入组件已支持混合方向。

## 段落分析

Core `BidiAnalysis` 拥有原始 `String` 和共享不可变分析资源，复制保留 source/paragraph lifetime，按文本和 `TextDirection` 比较语义值。`Auto` 按段落第一个强方向解析，没有强方向时使用 LTR；显式 LTR/RTL 指定段落基础方向。

公开查询始终使用逻辑 UTF-8 byte offset。非法方向不修改当前值；非法范围、跨段落 line 与非 scalar 边界拒绝。空段落、连续分隔符、末尾空行有效。`level_at`、script/paragraph spans 和语义比较不分配；创建分析和逐行 L1/L2 重排可以分配。

SheenBidi 3.0.0 使用 Unicode 17.0.0。内部传入 UTF-32 scalar 序列，并把 paragraph、level、script 与 visual line runs 转回 UTF-8，避免 L1 对多字节控制符的 continuation code units 产生不同 level、切开 UTF-8 scalar。算法资源在 source 和 scalar storage 释放前销毁，library 类型不泄漏至 Core header 或公开 API。

## 构建边界

`RynUI::SheenBidi` 只链接标准 C runtime。BUNDLED 的 archive、版本、fixture SHA256 和 license 集中锁定；SYSTEM 严格要求 3.0.0 config package、规范 target，启用测试时显式提供两个锁定 Unicode 数据文件。

上游 MSVC atomic 回退会包含 Windows SDK。BUNDLED 强制 C17，MSVC additionally 使用 `/experimental:c11atomics` 并执行标准 atomics configure probe；不支持时直接拒绝配置。实际 headless 构建及 Core include/link 守卫负责验证平台隔离。

## 逻辑 shaping

TextEngine 按 paragraph resolved level、script 和 fallback font 划分逻辑 runs，HarfBuzz 接收明确方向及 ISO 15924 script，并保留完整原始 source 的 offset/length 上下文。glyph 的顺序是各 run 的 shaper 输出，cluster 始终指向原始 UTF-8。数字的偶数 level 保持 LTR，即使段落基础方向为 RTL。

方向/isolate/连接控制符及 variation selectors 不要求 font coverage，不被替换为 U+FFFD；HarfBuzz 处理其 shaping 作用和不可见 glyph。真正缺字使用已有 fallback/replacement，保留原始 cluster。软折行之后的逐行视觉次序由后续 measurement 阶段消费，而非预先反转 source。

锁定 Noto Sans Arabic/Hebrew fixture 验证真实 script coverage、Arabic 邻接上下文与 lam-alef ligature、RTL bracket mirror、混合数字/fallback、控制符不可见、hard paragraph 和非法方向。fixture 不进入 Git，也不替代 native 系统字体验收。

## 验证与后续

TextMeasurement 的每行 glyph range 索引 `visual_glyphs`，再映射至 ShapedText 的原始 glyph。`visual_clusters` 保存逻辑 byte range、visual x/width 和该行 L1 后的 level。折行先遍历逻辑 cluster，行范围确定后调用该段落的 L1/L2 runs；每个 glyph 仅覆盖一次。GlyphScene 和文字 underline/strikeout 按同一视觉索引累计 pen，CPU metadata 不进入 renderer GPU ABI。旧人工 LTR measurement 缺少 visual metadata 时保留连续 glyph fallback。

TextState 方向变化触发 shaping/layout；width-only 更新复用 source 分析及 shaped 数据，只重新 measure/reorder。ellipsis 每个合法前缀独立执行对应方向的分析、shaping 和 measurement，并保留最长合法前缀及非单调宽度校验。

纯模型测试覆盖空段、分隔符、非法范围、复制 lifetime、重复赋值复用和查询零分配。全量 Unicode `BidiCharacterTest.txt` 与 `BidiTest.txt` 验证 paragraph base、逐行 levels 和 visual reorder，按 UTF-8 包装 API 运行。

本 change 的实际平台、preset、用例数量与阶段结果记录在 `openspec/changes/055-20261003-complete-native-bidirectional-text/evidence/`。字体 shaping、显示、编辑和真实窗口有各自独立验收任务。
