# Proposal

## Why

业务录入已有数字、选择和正在验收的单日期控件，仍缺少时间输入。补齐一个可独立使用的原生 TimePicker，让排班、筛选和设置页面可以选择时分秒，并沿用现有 Input、Theme 与保留弹层。

## What Changes

- 新增无日期、无时区的 typed 时间值与可空选择，严格的 24 小时 `HH:mm:ss` / 分钟精度 `HH:mm` 解析和格式化。
- 新增保留 Input 和真实时/分/秒列面板，支持受控/默认值及开合、清空、禁用、inputReadOnly、三尺寸、Input 四变体和状态。
- 支持有限步长、可选时间范围与禁用规则、键盘及滚轮列导航、默认确认提交和可选即时提交、typed footer slot/ref；取消与受控拒绝遵守明确的草稿合同。
- 接入 Theme、窗口层和 Modal/Spin/隐藏清理，验证实际 glyph、surface、hit clip 和 slot/输入身份。提供 API 文档与范围说明。

首版不包含 RangePicker、12 小时制、毫秒、自由格式/locale、自动 Now、预设列表、滚轮即提交、动画、Web/屏幕阅读器桥接。Gallery 与原生平台验收后续单独推进。

## Capabilities

### New Capabilities

- `native-time-picker`: 原生单时间输入、时间列选择、提交/取消、有限规则与主题。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

涉及公开 TimePicker/时间值 headers、model/host、Theme/runtime、窗口服务注册、CMake、focused tests 和 docs/time-picker.md。复用现有 Input、Button、窗口层与滚动/裁剪机制，不新增日期时间依赖，不修改 Core/renderer 边界或 logical scene/GPU ABI。基线为本地锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 time-picker 文档及 date-picker/time 样式；只承诺上面列出的原生首版。
