# Proposal

## Why

Input/TextArea 已具备单行、多行与 OTP 功能，但 TextEngine 仍拒绝同段 LTR/RTL，TextCaretMap 只接受递增 cluster，无法编辑阿拉伯语、希伯来语和中英文混合文本。用户要求已有组件原生桌面功能收尾；双向排版是 Input 完成及后续 Theme direction、Typography 多行编辑的共同依赖。

## What Changes

- 引入锁定的 SheenBidi 3.0.0（Unicode 17、Apache-2.0）作为平台无关 UAX #9 分段/level/逐行视觉重排基础，严格遵循 BUNDLED/SYSTEM，保留 Core 无 renderer/OS/SDL 依赖。
- TextEngine 按 bidi level、script 与 font 分段，向 HarfBuzz 明确传递方向/script，先按逻辑 cluster 折行，再按实际行重排；保留原 UTF-8 与 glyph cluster，控制字符不显示 replacement。
- TextMeasurement 增加 CPU 视觉 glyph/cluster 顺序；GlyphScene 共用布局结果，既有 GPU ABI 与上传事务不变。Text/Title/Paragraph/Input 家族增加 typed reactive TextDirection（Auto/LTR/RTL）段落入口。
- TextCaretMap 同时提供逻辑 byte 查询与视觉 stops，包含 run/软换行边界 affinity；Input/TextArea 的左右/Home/End/上下/指针命中及选择覆盖按视觉位置，删除/clipboard/history 保留逻辑 grapheme 合同。
- 单/多行 selection 与 composition 支持每行多个不连续视觉覆盖，selected view 共享一次 shape，正确同步 IME area、Password mask、OTP 与 retained identity。
- 更新 Gallery/文档，完成共同回归和 Windows 真实窗口证据；Linux 独立待实际机器验收。

## Capabilities

### New Capabilities

- `bidirectional-text`: Unicode 段落分析、direction/script shaping、按行视觉顺序、合法 grapheme caret/selection 与原生输入的双向文字合同。

### Modified Capabilities

无。当前 `openspec list --specs --json` 主规格目录为空；此前 TextEngine/Input 相关设计与实现作为兼容基线，本 change 单独建立新增合同。

## Impact

涉及 dependency lock/notice/guards、font shaping、text engine/caret map/state/scene service、logical GlyphScene、公开 Text/Typography/Input headers、组件交互及 Gallery。新依赖源 archive SHA256 已核对为 `86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786`，tag commit `cfe430e7375a7845b679adae9d51dac6deaa8858`。

风险集中在 logical/visual 索引、ligature/多字体边界、折行后的 L1/L2、selection 非连续 coverage、IME 临时 display byte 映射及零分配 idle；各阶段用 conformance/明确场景与既有 allocation 回归验证。非目标为 vertical text、Web dir/DOM/CSS API、自动系统语言切换、新 renderer/backend、彩色 emoji 和人工 OS 候选窗口操作。此处是规划，不代表双向功能已经实现。
