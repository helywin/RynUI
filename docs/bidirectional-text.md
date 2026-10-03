# 双向文本

055 已接入原生桌面双向文本的段落分析、逻辑 shaping、逐行视觉布局、CaretMap、Input/选择场景与公开方向配置。实际共同/原生验收按 change 的独立任务记录。

`<ryn/text_direction.hpp>` 提供 `TextDirection { Auto, LeftToRight, RightToLeft }`；TextProps、TypographyProps、TitleProps 与 InputPropsBase 的 `.direction(Prop<TextDirection>)` 默认 Auto，Password/Search/TextArea 继承。动态改变仅刷新相关文本、必要布局和 input area，保留 editor/ref/scene/selection/history/session 与 slots；非法初值在资源创建前拒绝，非法 reactive 值保留已接受方向，后续合法值可继续更新。Typography 编辑器继承同一方向。该属性决定段落基础方向，不改变容器/affix 排列；OTPDirection 继续控制格子组方向，格子文本按 Auto 分析。

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

原生默认系统链通过平台字体发现补足 Arabic/Hebrew coverage：Windows 在 Segoe UI Variable/YaHei 后按缺失 coverage 查询静态 Segoe UI、Tahoma、Arial；Linux 对 Fontconfig generic sans-serif 增加 `ar`/`he` 查询。文件与 face 不写死路径，Core 仍统一使用 FreeType/HarfBuzz shaping。Windows Debug/Release 已完成十组系统/指定缩放的 D3D12/DXIL 窗口、原生 clipboard/session/input area 与 190 张 GPU readback 核验，记录见该 change 的 `evidence/windows/README.md`；Linux 原生项待实际机器执行。

## 验证与后续

CaretMap 的 `stops()` 按逻辑 byte/affinity 排序，`line_stops()` 按视觉 x/byte/affinity 排序。`Upstream` 来自前一逻辑 grapheme 的末端，`Downstream` 来自后一 grapheme 的起点；同一 byte 可以在方向交界或软换行具有两个位置。命中相同 x 时选择最早逻辑 byte，保持确定行为；视觉 adjacent/line edge/2D hit 和覆盖遍历均不分配。

`visit_coverage` 遍历预备的 grapheme 视觉片段，将相邻片段合并，保留双向文本形成的间隙。单行 caret 与 layout 共用纯几何构造，ligature 按合法 grapheme 等比例分割；glyph 数量不决定编辑有效位置。紧急折行不能切开 grapheme，即使 fallback 和连接控制符让它包含多个 shaper clusters。准备失败保持已发布的 logical/visual/coverage/line 数据及 revision；MSVC Debug 分配探针下的临时容器采用可抛异常构造和已有 scratch storage，避免 noexcept iterator bookkeeping 终止进程。

TextMeasurement 的每行 glyph range 索引 `visual_glyphs`，再映射至 ShapedText 的原始 glyph。`visual_clusters` 保存逻辑 byte range、visual x/width 和该行 L1 后的 level。折行先遍历逻辑 cluster，行范围确定后调用该段落的 L1/L2 runs；每个 glyph 仅覆盖一次。GlyphScene 和文字 underline/strikeout 按同一视觉索引累计 pen，CPU metadata 不进入 renderer GPU ABI。旧人工 LTR measurement 缺少 visual metadata 时保留连续 glyph fallback。

TextState 方向变化触发 shaping/layout；width-only 更新复用 source 分析及 shaped 数据，只重新 measure/reorder。ellipsis 每个合法前缀独立执行对应方向的分析、shaping 和 measurement，并保留最长合法前缀及非单调宽度校验。

Input/TextArea 共用 run/line affinity；左右箭头沿视觉 stops，Home/End 使用当前行物理边缘，primary Home/End 使用逻辑文档端点。指针保留命中 stop 的精确 affinity。Shift 保留逻辑 anchor，删除、clipboard 与 history 使用原始逻辑 UTF-8；Password mask 在显示 grapheme 与 committed byte 之间转换。

选区和 preedit 使用每行不连续 coverage。GlyphPrimitive 为可见实例保存 CPU cluster byte/line/x metadata，selected view 同时检查逻辑 ownership 与当前视觉片段，防止相邻未选 glyph 的 bearing 越界染色。clip/scroll 只 patch geometry，所有层共享 shaping；GPU ABI 不变。单行保留最低一个 selection、两个 overlay 槽位，双向额外片段按需要扩展；静止与既有连续选区保持局部更新合同。

纯模型测试覆盖空段、分隔符、非法范围、复制 lifetime、重复赋值复用和查询零分配。全量 Unicode `BidiCharacterTest.txt` 与 `BidiTest.txt` 验证 paragraph base、逐行 levels 和 visual reorder，按 UTF-8 包装 API 运行。

本 change 的实际平台、preset、用例数量与阶段结果记录在 `openspec/changes/055-20261003-complete-native-bidirectional-text/evidence/`。字体 shaping、显示、编辑和真实窗口有各自独立验收任务。
