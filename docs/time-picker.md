# TimePicker

原生单时间输入，值为一天内的 `TimeOfDay`（无日期、时区或系统时钟）。设计参照本地 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/time-picker` 与 `components/date-picker/style`；不引入 Day.js 或其他依赖。

## 值与提交

`TimeOfDay{hour, minute, second}` 只允许 0–23 / 0–59 / 0–59，非法构造抛出 `std::invalid_argument`。`TimePickerValue` 为 `std::optional<TimeOfDay>`，空值表示未选择。`parse_time_of_day` 严格接受 ASCII `HH:mm:ss`，分钟模式严格接受 `HH:mm`；不接受空格、单数字、额外字符或越界时间，失败返回 `nullopt`。`format_time_of_day` 使用同一固定格式。

默认确认模式：输入只更新文本草稿；列选择只更新待确认时间；Enter 或 OK 请求合法值并关闭，Cancel、Escape、外部关闭放弃面板改动。非法/未完成文本不发布，失焦恢复最后接受值。preedit 留给 Input/IME，不触发时间提交。清空请求空值。`needConfirm(false)` 在合法列选择时请求值且保留弹层；滚轮只滚动列，绝不选值或提交。

`value(Prop<TimePickerValue>)` 与 `open(Prop<bool>)` 受控，不能分别和 `defaultValue` / `defaultOpen` 混用。回调是请求，父级未回写时已接受值与关闭后的显示仍遵守父级 authority。外部受控值即使不符合当前步长/范围/禁用规则也保留，不静默钳位。分钟模式显示省略秒，外部非零秒仍保存在公开值中；所有新的分钟精度提交均为零秒。无值面板从 `defaultPanelValue` 或 00:00:00 开始，不读取系统时间。

## 规则与列

`TimePickerRules` 包含 `hour_step`（1–24）、`minute_step` / `second_step`（1–60），各列从零开始按模数筛选。`min` / `max` 为含端点、不跨午夜的范围，要求 min ≤ max。`disabled(TimeOfDay)` 为纯判定函数，返回 true 表示不可选；规则可通过 `Prop` 更新。禁止函数内修改组件状态或执行 I/O。

小时行可用性检查该小时是否存在合法完成，分钟行检查当前小时/该分钟是否存在合法秒。选中时优先保留其他列；若不可选则使用该前缀下最早合法完成。秒列只改变秒。每次搜索至多遍历一天的 24×60×60 域，全禁用时不提交、不循环重试。导航在列端点停止。

## 首版范围

保留 Input、时/分/可选秒列和 typed `TimePickerFooter`，安全 `TimePickerRef` 复用 Input 的 focus/blur/select/bound 生命周期。支持 clear、disabled、inputReadOnly、三尺寸、Input 四变体及状态。footer 只挂载一次，普通更新保持编辑器及子内容身份。

不包含 RangePicker、12 小时制、毫秒、自由格式/locale、Now、预设、滚轮即提交、动画、Web 或屏幕阅读器桥接。Gallery、原生 GPU、Linux、系统 IME 验收独立推进，headless 合同不替代这些证据。
