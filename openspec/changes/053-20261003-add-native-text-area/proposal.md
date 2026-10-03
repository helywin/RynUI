# Proposal

## Why

单行 Input 家族已完成 052 收尾，但没有原生 TextArea；TextEditor 去掉 CR/LF，TextCaretMap 拒绝多段落，无法承载多行编辑。补齐这个共享基础，才能继续完成 Input 家族和 Typography 多行原地编辑。

## What Changes

- 新增 typed TextAreaProps、TextArea、TextAreaRef，复用 Input 的值、焦点、原生提示、变体、Theme、计数、清空和编辑事务。
- 编辑器增加显式多行模式，统一 CRLF/CR 为 LF，保留历史、controlled、grapheme 边界与原子 formatter。
- 增加行布局光标映射、换行/软折行、上下/Page/Home/End 导航、跨行选择、垂直滚动和对应 IME 区域。
- 增加 reactive rows、autoSize(minRows/maxRows)、wrap、原生 resize 配置和尺寸通知；保持挂载/editor/scene 身份。
- Gallery 与原生 GPU 验收覆盖四变体、统计/清空、resize、缩放、滚动及销毁。

## Capabilities

### New Capabilities

- `native-text-area`: 原生多行编辑、尺寸、导航与 retained 呈现的完整合同。

### Modified Capabilities

无已归档主规格变更；单行 Input 既有合同保持兼容。

## Impact

公开 include/ryn/text_area.hpp、input/text_editor、text/text_caret_map、text_scene_service、InputComponentHost、LayoutEngine、Gallery、测试和 Input 文档。不增加第三方依赖，不将 SDL/renderer 引入 Core。

范围是原生桌面 TextArea；DOM/CSS/React、HTML form/autofill、Web resizableTextArea/nativeElement 不移植。OTP、混合双向文字及 Typography 的实际多行编辑接入继续由后续 change 完成，仍属于用户要求的组件收尾范围。风险集中在软折行边界的 caret affinity、跨行字形选择、autoSize 测量依赖和回调卸载；按独立回归验证。Windows 本机实际验收，Linux 窗口/GPU 单独保留。
