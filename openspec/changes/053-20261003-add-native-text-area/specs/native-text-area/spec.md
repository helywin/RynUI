# Spec Delta

## Purpose

定义 RynUI 原生桌面多行输入框的稳定 typed API、换行编辑、行间导航、自动尺寸、滚动和交互合同，使 Input 家族及后续 Typography 编辑可以复用相同的长期编辑状态、主题和平台输入会话。

## ADDED Requirements

### Requirement: 多行编辑兼容共用输入合同

TextArea SHALL 支持 typed/reactive 共用 Input 值、placeholder、ref、焦点、系统提示、disabled/readOnly、四变体、三个尺寸、统计/软硬上限和清空。多行初值、粘贴、IME commit、formatter 与 authoritative 回写 MUST 将 CRLF/CR 规范为 LF；普通 Input/Password/Search SHALL 继续去掉 CR/LF。

#### Scenario: 规范化与历史
- **WHEN** TextArea 收到包含 CRLF、CR、LF、CJK 和组合 grapheme 的编辑
- **THEN** 值只保留 LF，maxLength 不拆 grapheme，单次 formatter 与 undo/redo 保留多行原子事务，controlled 与卸载/异常合同保持成立

### Requirement: 实际行布局与导航

TextArea SHALL 根据 LF 与可配置 wrap 生成行布局，支持二维点击/拖动、左右、上下、PageUp/PageDown、行 Home/End、primary Home/End 及 Shift 扩展选择。上下移动 SHALL 保留期望 x；软折行边界 SHALL 有明确上游/下游位置。跨行选择和组合文本 SHALL 与实际行位置一致。

#### Scenario: 软折行与空行
- **WHEN** 文字包含空行、末尾 LF、emoji、ligature 并在窄宽度折行，用户点击及跨行移动/选择
- **THEN** 光标始终落在完整 grapheme 边界和目标视觉行，选择覆盖正确行段，resize 后身份与逻辑选择保留

### Requirement: 尺寸配置与自动高度

TextArea SHALL 支持 reactive 正整数 rows、autoSize enabled/minRows/maxRows、wrap、None/Vertical/Horizontal/Both resize 与原生尺寸回调。默认 rows=4/wrap=true/resize=Vertical。autoSize SHALL 按实际行数在 minRows/maxRows 内变化并禁用手工 resize；外部 LayoutStyle 尺寸约束 SHALL 保持优先。非法配置 MUST 拒绝且不发布部分状态。

#### Scenario: 内容和宽度变化
- **WHEN** 内容、宽度、字体、rows 或 autoSize 边界发生变化
- **THEN** 高度按实际行布局和约束更新，超出可见高度部分可滚动，尺寸回调报告实际逻辑宽高且允许卸载，editor/ref/scene 内容挂载不重建

### Requirement: 原生滚动和 resize

TextArea SHALL 在输入/导航时露出 caret，并允许 wheel/Page 滚动及正常 pointer capture 的配置方向 resize。readOnly SHALL 允许选择/复制/滚动，disabled SHALL 禁止编辑/resize。scroll SHALL 对齐 physical pixels 且不为纯滚动重复 shape/raster，分支撤销/失焦/资格变化/卸载 SHALL 取消相关捕获。

#### Scenario: 长文与拖动
- **WHEN** 长文超出 rows 上限，用户滚动/移动 caret 或拖动 resize grip，然后禁用或卸载
- **THEN** 内容与选区保留，输入区域随可见 caret 更新，没有旧 capture、editor、scene、ref 或 deadline 残留

### Requirement: 键盘和组合输入

TextArea SHALL 在普通 Enter 插入 LF，在 primary Enter 调用 onSubmit，在 Tab 使用正常焦点遍历；active preedit SHALL 拥有编辑导航和 Enter，旧 stamp SHALL 不得提交。原生输入区域 SHALL 使用多行可见 caret 的坐标变换。

#### Scenario: preedit 跨行
- **WHEN** TextArea 聚焦并在另一行组合输入，然后提交/取消、切换系统提示或失活
- **THEN** preedit 只在提交时进入计数和历史，stamp 更新与取消正确，caret/selection/scroll 与平台输入区域保持一致

### Requirement: 验收与保留场景

TextArea SHALL 使用共同 logical scene 和 renderer 上传事务，普通属性更新 SHALL 不执行无关内容；平台通用合同 SHALL 在实际支持平台验证，Windows/Linux 窗口与 GPU 证据 SHALL 独立记录。

#### Scenario: 原生窗口
- **WHEN** 在对应平台以 Debug/Release 运行四变体/Theme、不同缩放、输入/选择/clear、scroll/resize 与 idle/dispose
- **THEN** 测试日志、退出码、GPU 读回尺寸/hash 与实际截图可复核；没有对应平台运行证据时不得勾选该平台验收
