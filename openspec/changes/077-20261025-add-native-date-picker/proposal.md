# Proposal

## Why

业务页面已有 Input、Select、InputNumber 和 Table，仍缺少能够选择日期的原生控件。先提供单日期的可用首版，让录入、筛选和设置页面可以使用同一套保留组件与 Theme。

## What Changes

- 新增无时区的 typed 公历日期值、可空单选值与 DatePicker Props/ref；支持受控/默认值、受控开合、清空和固定 ISO `YYYY-MM-DD` 输入。
- 新增真实月历弹层，支持跨月/跨年导航、指针与键盘选择、日期范围及 disabledDate、三尺寸、已有 Input 变体/状态、typed footer slot。
- 接入 Theme、窗口层、焦点恢复和 Modal/Spin 边界，保留输入与 slot 身份；补齐实际节点/场景与交互测试。
- 提供独立 API 文档与首版限制。RangePicker、多选、周/月/季/年 picker、时间选择、复杂格式/locale、预设列表、动画及 Web API 留待后续独立增量。

## Capabilities

### New Capabilities

- `native-date-picker`: 原生单日期录入、月历选择、范围限制、保留弹层与主题。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

涉及公开日期/DatePicker headers、model/host、Theme/runtime、组件注册与 CMake、focused tests 和 docs/date-picker.md。不引入 Day.js 或第三方日历依赖，不修改 logical scene/GPU ABI。参考已核对的本地 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 date-picker 文档与 style；首版只实现明确列出的原生合同。Gallery 和原生平台矩阵独立推进，不能由 headless 测试代替。
