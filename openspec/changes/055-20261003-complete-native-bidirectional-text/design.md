# Design

## Context

动机见 proposal。现有 TextEngine 检查强方向混合并直接失败；FontRuntime 使用 `hb_buffer_guess_segment_properties`，run 主要按 fallback font 划分。measure 按 shaped glyph连续范围折行，GlyphScene逐个累计 advance，CaretMap拒绝 RTL和逆序cluster。Input 单行selection仅一个矩形，TextArea每行仅一个clip；二者不能表达双向选择的视觉空隙。

## Goals / Non-Goals

Goals：UAX #9 的段落分析与逐行重排由一个可靠的 Core依赖完成，glyph/caret/coverage消费同一CPU几何，文本编辑仍以逻辑UTF-8为唯一事实来源。TextDirection只规定段落基础方向，UI容器/affix的方向仍由各组件布局及后续Theme defaults负责；OTPDirection继续控制格子组排列。

Non-Goals：见proposal。采用水平排版，不增加unicode editor之外的视觉字符串副本，不依赖Win32 Uniscribe/DirectWrite shaping或Linux专用库。

## Decisions

### 1. 锁定 SheenBidi，包装所有权及字节范围

使用 SheenBidi 3.0.0、Unicode 17（源码 Tools/Unicode/BidiCharacterTest.txt已核实版本）、Apache-2.0。它只依赖标准C，已提供UTF-8 paragraph levels、line runs和script locator。BUNDLED锁定tag/archive SHA，SYSTEM严格要求版本及显式package，无回退。Unicode conformance数据单独锁定license/hash，validation fixtures保持构建树内。禁止修改用户的 unrelated token updater。

Core新增 BidiAnalysis值：拥有原始String与不可变共享分析资源，复制保留安全lifetime，按内容/direction比较语义值；查询返回UTF-8边界/level/base level和script runs。SheenBidi类型只在内部实现使用，不泄漏公开headers，paragraph/algorithm资源按正确顺序释放。空文本/空行在包装层有效；非法范围/非scalar边界拒绝且不发布部分结果。line创建应用L1/L2，不能使用whole-paragraph视觉次序进行软折行。

备选：自写简化RTL检测不能满足弱类型、括号/isolate及逐行规则；ICU/FriBidi带来更多构建或license负担。HarfBuzz负责shaping，不替代UBA。

### 2. 逻辑 shaping 与视觉 measurement 分离

FontRuntime增加internal方向/script options，默认保留既有调用。TextEngine按paragraph resolved level、script locator、font coverage划分逻辑runs；HarfBuzz收到整份source的offset/length上下文及明确direction/script，防止跨script猜测或fallback导致连接丢失。Unicode格式控制由shaping/UBA处理，不触发missing-glyph replacement；真正缺字保持原cluster映射。

ShapedText保留逻辑paragraph/runs与glyph。TextMeasurement增加visual glyph indices/cluster boxes以及实际line ranges，先按逻辑cluster advance确定合法break，再由同一BidiAnalysis获取该行视觉runs，按run方向排列cluster并保留cluster内部shaper输出顺序。每glyph只覆盖一次；GlyphScene/text decorations与CaretMap读取该视觉几何。旧人工LTR shaped fixtures可使用连续glyph fallback，不要求构造新分析对象。CPU metadata不进入GPUABI；glyph atlas、packed上传与owner/epoch不变。

TextState将direction变化标为shape/layout invalidation，width只重新measure/reorder，不重复shape。ellipsis的每个合法前缀candidate独立分析/shaping/measure，suffix仍原子，沿用最长合法前缀与非单调宽度校验。

### 3. 双索引 caret 与不连续 coverage

CaretMap发布按byte/affinity排序的逻辑查询与按line/x排序的视觉stops，不再假设两者顺序相同。边界upstream来自前一逻辑grapheme，downstream来自后一grapheme，run/软换行交界可具有不同x/line；相同位置按确定byte/affinity顺序处理。ligature按grapheme划分advance，RTL按反向比例映射。visual adjacent/line edge/nearest查询及range coverage遍历不分配，generation revision验证沿用。

Input单行及TextArea共用affinity。plain左右按视觉adjacent；无Shift的非空选择先折叠至对应视觉边缘。Home/End使用视觉行边缘，primary Home/End保留文档逻辑起止，上下/Page保留期望x。Backspace/Delete/undo/clipboard仍逻辑grapheme事务。pointer和IME area引用确切affinity；display模型的composition/mask字节转换先完成，再应用视觉映射。

selection/preedit使用每行visual cluster coverage，连续片段合并、视觉空隙保留；每个selected glyph的clip按其逻辑cluster与已选片段确定，必要时增加CPU glyph coverage metadata，selected view共享TextState，不为片段重新shaping。retained selection/overlay范围按实际片段数更新，composer拓扑变动仍经现有needs_rebuild/dirty唤醒。

### 4. API 与兼容边界

新增共享 `<ryn/text_direction.hpp>` 的 `TextDirection { Auto, LeftToRight, RightToLeft }`，TextProps/TypographyProps/InputPropsBase的 `.direction(Prop<TextDirection>)`；Input子类型继承，默认Auto。无强方向默认LTR保持旧文本，现有纯RTL变为可编辑，混合文本不再报mixed_direction_unsupported。非法reactive枚举先验证再接受，可继续合法更新。Theme公共direction defaults在后续ConfigProvider收尾定义，本change不提前复制provider。

## Risks / Trade-offs

- [逻辑/视觉范围不连续] → conformance与含embedding/isolate/Arabic/Hebrew/数字/括号/换行场景逐一验证，measure与GlyphScene通过同一visual order。
- [ligature和fallback边界] → shape保留source上下文与cluster，caret使用合法grapheme，selection检查多片段及原逻辑clipboard子串。
- [历史allocation/idle回退] →只在source/direction/width更新分析或measure，reuse query buffers；完整headless及input_scene_allocation必须通过。
- [平台字体不同] →共用test使用锁定fixture，Windows native使用系统chain，缺字按fallback而不更改编辑合同；Linux只在实际机器验收。
- [既有scene与API兼容] →新增CPU元数据不改GPUABI，prefix/ref/scene不重建，保留LTR/CJK与Password/OTP/TextArea回归。

## Migration Plan

按tasks分阶段提交：依赖/analysis → shaping/measure/GlyphScene → caret/input coverage → API/Gallery/共同回归 → Windows独立证据。正式preset为Ninja Multi-Config，Windows MSVC，common用windows-msvc-headless Debug/Release，native用windows-msvc；Linux独立linux-native Debug/Release项保留待实际机器。无存储迁移；回退对应change提交即可恢复旧实现。

## References

2026-10-03核对：[Unicode 17 UAX #9](https://www.unicode.org/reports/tr9/)、[SheenBidi v3.0.0](https://github.com/Tehreer/SheenBidi/tree/v3.0.0)、[HarfBuzz segment properties](https://harfbuzz.github.io/harfbuzz-hb-buffer.html)、[HarfBuzz source context](https://harfbuzz.github.io/adding-text-to-the-buffer.html)。来源只支持设计；不作为实现或平台验收证据。
