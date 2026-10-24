# TimePicker

原生单时间输入，值为一天内的 `TimeOfDay`（无日期、时区或系统时钟）。设计参照本地 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/time-picker` 与 `components/date-picker/style`；不引入 Day.js 或其他依赖。

## 值与提交

`TimeOfDay{hour, minute, second}` 只允许 0–23 / 0–59 / 0–59，非法构造抛出 `std::invalid_argument`。`TimePickerValue` 为 `std::optional<TimeOfDay>`，空值表示未选择。`parse_time_of_day` 严格接受 ASCII `HH:mm:ss`，分钟模式严格接受 `HH:mm`；不接受空格、单数字、额外字符或越界时间，失败返回 `nullopt`。`format_time_of_day` 使用同一固定格式。

默认确认模式：输入只更新文本草稿；列选择只更新待确认时间；Enter 或 OK 请求合法值并关闭，Cancel、Escape、外部关闭放弃面板改动。非法/未完成文本不发布，失焦恢复最后接受值。preedit 留给 Input/IME，不触发时间提交。清空请求空值。`needConfirm(false)` 在合法列选择时请求值且保留弹层；滚轮只滚动列，绝不选值或提交。

`value(Prop<TimePickerValue>)` 与 `open(Prop<bool>)` 受控，不能分别和 `defaultValue` / `defaultOpen` 混用。回调是请求，父级未回写时已接受值与关闭后的显示仍遵守父级 authority。外部受控值即使不符合当前步长/范围/禁用规则也保留，不静默钳位。分钟模式显示省略秒，外部非零秒仍保存在公开值中；所有新的分钟精度提交均为零秒。无值面板从 `defaultPanelValue` 或 00:00:00 开始，不读取系统时间。

受控 `open=true` 时，Escape、OK、Cancel 和外部点击只发关闭请求；父级拒绝则保持打开，用户可重试。Modal/Spin、祖先隐藏、disabled 和窗口失活属于强制边界，独立抑制旧弹层；边界解除后需点击/键盘打开，或父级显式 false→true，才恢复。每个新打开周期重新跟踪焦点，程序化打开不要求先聚焦 Input。

通过 String field adapter 与 Form 组合时，显式 reset 同时恢复 TimePicker model、待确认值、输入文本并取消 preedit，即使 field 的接受值没有变化也生效；不会发布未提交草稿。普通 validation/help/Theme 更新保持正在编辑的草稿。reset 命令与 ref 都受组件 generation 保护。

## 规则与列

`TimePickerRules` 包含 `hour_step`（1–24）、`minute_step` / `second_step`（1–60），各列从零开始按模数筛选。`min` / `max` 为含端点、不跨午夜的范围，要求 min ≤ max。`disabled(TimeOfDay)` 为纯判定函数，返回 true 表示不可选；规则可通过 `Prop` 更新。禁止函数内修改组件状态或执行 I/O。

小时行可用性检查该小时是否存在合法完成，分钟行检查当前小时/该分钟是否存在合法秒。选中时优先保留其他列；若不可选则使用该前缀下最早合法完成。秒列只改变秒。每次搜索至多遍历一天的 24×60×60 域，全禁用时不提交、不循环重试。导航在列端点停止。

## 示例

在窗口的 `Content` 中调用；`value` Signal 与 ref 由页面持有。相同 API 示例由 `time_picker_public_api_tests.cpp` 编译和链接。

```cpp
void time_picker_example(ryn::Signal<ryn::TimePickerValue> value, ryn::TimePickerRef reference) {
    using namespace ryn;
    TimePickerRules rules;
    rules.minute_step = 15;
    rules.min = TimeOfDay{8};
    rules.max = TimeOfDay{18};
    TimePicker(TimePickerProps{}.value(value).ref(reference).precision(TimePickerPrecision::Minute)
        .rules(rules).onChange([value](TimePickerValue next) mutable { value.set(next); })
        .layout(LayoutStyle{}.width(dp(240))),
        TimePickerFooter{[] { Text(u8"Business hours: 08:00–18:00"); }});
}
```

## 外观与交互

`size` 为 Small/Middle/Large，`variant` 为 Outlined/Filled/Borderless/Underlined，`status` 使用现有 `InputStatus`。`inputReadOnly` 只禁止文本编辑，仍可使用面板；`disabled` 同时阻止面板、编辑及清空。

点击或 Down/Up 打开；面板打开时 Left/Right 切换列，Up/Down 跳过禁用项，Home/End 定位首末可选项，Enter 确认，Escape 取消。footer 内的编辑器、按钮、Table 与子 popup 保留其原生行为；preedit 优先交给当前编辑器。Tab 可访问 footer 和确认/取消按钮，离开整个组件后取消未提交改动。`TimePickerRef` 的 focus 不隐式打开。

`ThemeConfig::time_picker` 提供 `tokens`、`seed` 和 `algorithm`。panel 的 colors/metrics/effects/order 独立追踪；`column_width`、`column_height`、`cell_height`、padding、footer_padding、gap、border_radius、border_width、font_size、line_height、font_family、shadow、z_index_popup 均为 typed component token。Input 三尺寸、变体及错误状态继续复用 Input token；time_picker 的 input_background/hover_border/active_border/active_outline 覆盖对应 selector 外观。局部 seed 仅在 `algorithm=true` 时生效；非法最终派生尺寸或不可渲染字号在发布前拒绝。

## 首版范围

保留 Input、时/分/可选秒列和 typed `TimePickerFooter`，安全 `TimePickerRef` 复用 Input 的 focus/blur/select/bound 生命周期。支持 clear、disabled、inputReadOnly、三尺寸、Input 四变体及状态。footer 只挂载一次，普通更新保持编辑器及子内容身份。

不包含 RangePicker、12 小时制、毫秒、自由格式/locale、Now、预设、滚轮即提交、动画、Web 或屏幕阅读器桥接。原生 GPU、Linux、系统 IME 与辅助技术验收独立推进，Recording/headless 合同不替代这些证据。

## Gallery 与 Form String 适配

Gallery 的 **TimePicker** 目录直接进入导航样例之后的真实时间选择流程。独立样例固定分钟精度、初值与空值面板 `08:00`，允许 `08:00` 至 `18:00`、15 分钟步长，排除 12 时。确认/即时模式按钮改变列选择的提交方式；锁定已保存值按钮拒绝回写选择请求，反馈同时显示实际接受值和最后请求。文本只读按钮只禁止键入，仍允许列选择及清空。面板保留备注 Input，滚轮只滚动对应列。

另一个独立 Form 固定秒精度、String 初值 `08:00:00`，允许 `08:00:00` 至 `18:00:00`，排除 12 时。字段 required，非空程序值也由 validator 检查严格 `HH:mm:ss` 与声明范围。只在 TimePicker 发布合法接受请求时写回字段；输入草稿与待确认列值不会直接进入 FormResult。

```cpp
// field、reference 由页面持有；在 FormItem 的 content 内取得注册代际回调。
const auto change = field.onChange();
TimePicker(TimePickerProps{}
    .value(bind([field] { return parse_time_of_day(field.get().bytes()); }))
    .onChange([change](TimePickerValue value) {
        change(value ? format_time_of_day(*value) : String{});
    })
    .defaultPanelValue(TimeOfDay{8})
    .precision(TimePickerPrecision::Second)
    .status(field.status())
    .disabled(field.uneditable())
    .inputReadOnly(field.readOnly())
    .ref(reference));
```

FormItem 使用 `.control(reference)` 连接实际 `TimePickerRef`，失败提交聚焦该 picker；Form 只读/禁用通过 `uneditable()` 阻止整个选择器，不能仅使用 `inputReadOnly`。保存、重置、编辑门禁及帮助按钮均在 Form 外，禁用后仍可恢复。保存结果来自实际 `FormResult.values`，reset 恢复初值并丢弃完整/未完成草稿及 preedit，包括接受值未变且输入仍聚焦的情况；普通帮助、验证和 Theme 更新保留编辑状态。Form 的四类 typed 值没有扩展。

聚焦入口为 `rynui_token_gallery_frame_tests --time-picker-only`，并纳入完整 frame。平台通用 Recording 验证与最终原生验收分别记录在 [079](../openspec/changes/079-20261025-add-form-and-picker-gallery/tasks.md)，不将启动 smoke 或合成 Composition 事件记为系统 IME 验收。
