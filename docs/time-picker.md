# TimePicker

`TimeOfDay` 表示一天内的本地钟面值，没有日期、epoch、时区或隐式系统时钟。原有 `TimeOfDay{hour, minute, second}`、`TimePickerValue = std::optional<TimeOfDay>` 和单值 `TimePicker` 调用继续有效；第四个构造参数和 `millisecond()` 提供 0–999 的整数毫秒。非法值在构造时拒绝。

```cpp
#include <ryn/rynui.hpp>

void time_field(ryn::Signal<ryn::TimePickerValue> value, ryn::TimePickerRef reference) {
    using namespace ryn;
    TimePickerRules rules;
    rules.minute_step = 15;
    rules.min = TimeOfDay{8};
    rules.max = TimeOfDay{18};
    TimePicker(TimePickerProps{}.value(value).ref(reference)
        .precision(TimePickerPrecision::Minute).rules(rules)
        .onChange([value](TimePickerValue next) mutable { value.set(next); })
        .layout(LayoutStyle{}.width(dp(240))),
        TimePickerFooter{[] { Text(u8"Business hours: 08:00–18:00"); }});
}
```

`value/defaultValue`、`open/defaultOpen` 分别互斥。受控事件只请求应用更新；拒绝回写时，accepted 值保持外部 authority。打开后列选择先修改 pending，`needConfirm(true)` 的 OK 或 Enter 才请求值；`needConfirm(false)` 选择合法行时请求值。非法或不完整输入不会提交旧 pending。清空请求 `nullopt`。显式外部值即使不满足新规则也不会被静默钳位。

`precision` 支持 Minute、Second、Millisecond。各步长分别为 1–24、1–60、1–60、1–1000，锚定零；非整除步长仍严格筛选所有边界。`min/max` 为含端点且不跨午夜的有序范围。`disabled` 判断完整值；`disabled_hours`、`disabled_minutes(hour, minute)`、`disabled_seconds(hour, minute, second)` 和 `disabled_milliseconds(hour, minute, second, millisecond)` 表达依赖前列的禁止规则。规则更新先验证，再发布。

`use12Hours(true)` 实际显示 12 行小时与 AM/PM 列，00/12 点正确映射为 12 AM/PM，内部仍保留 0–23 时。`format` 的验证格式支持 H/HH、h/hh、m/mm、s/ss、SSS、a/A 和 `[literal]`；12 小时格式必须包含 meridiem。无格式时按精度和时制选择默认格式。`parse_time_of_day(text, format, locale)` 与 `format_time_of_day(value, format, locale)` 使用同一规则；旧精度重载仍是严格 HH:mm 或 HH:mm:ss，新增 Millisecond 为 HH:mm:ss.SSS。locale 可配置 AM/PM 与动作文字。

```cpp
void precise_time(ryn::Signal<ryn::TimePickerValue> value) {
    using namespace ryn;
    TimePickerRules rules;
    rules.millisecond_step = 125;
    rules.disabled_milliseconds = [](int hour, int minute, int second, int ms) {
        return hour == 12 && minute == 0 && second == 0 && ms == 500;
    };
    TimePicker(TimePickerProps{}.value(value).use12Hours()
        .precision(TimePickerPrecision::Millisecond).format(u8"h:mm:ss.SSS A")
        .rules(rules).clock([] { return TimeOfDay{9, 30, 0, 125}; }).showNow()
        .onChange([value](TimePickerValue next) mutable { value.set(next); }));
}
```

Now 必须提供 `.clock(...)`，由业务明确本地时间；不把单调动画时间当作钟面时间。presets 使用唯一 key、标签和惰性值回调，点击时读取。`changeOnScroll(false)` 默认只滚动实际命中的列；true 时滚轮选择相邻可用行，再遵守 needConfirm。不会在 wheel 中重新挂载行，也不消费面板外滚轮。列、富单元格与 footer 保留身份；毫秒和 AM/PM 列按需要首次挂载。

`cellRender` 接收响应式 `TimePickerCellInfo`，返回 typed `TimePickerCell`，每个行内容只构造一次；首次构造即提供实际列、显示值和范围端点，后续通过 Prop 更新。`panel` 替换列区域，`TimePickerFooter` 放置额外内容，`suffixIcon` 提供保留图标。自定义 panel 可使用应用的 Signal 及公开值回调组织选择。所有视觉继续由 TimePicker/Input Theme 和 Component Token 控制，LayoutStyle 只控制外部尺寸。RTL 反转列布局及左右键，placement 支持四角和窗口内溢出回退。开合使用有限 NavigationTransition；reduced motion、隐藏或窗口失活会结束动画。

原生范围组件命名为 `TimeRangePicker`，对应上游 `TimePicker.RangePicker`。范围值 `TimePickerRangeValue` 有独立可空 start/end；`TimeRangePickerProps::picker` 接收共用的 TimePickerProps，避免与日期 RangePicker 同名。

```cpp
void time_range(ryn::Signal<ryn::TimePickerRangeValue> value) {
    using namespace ryn;
    PickerRangePolicy policy;
    policy.allow_empty = {true, false};
    TimeRangePicker(TimeRangePickerProps{}.value(value).policy(policy)
        .picker(TimePickerProps{}.use12Hours().precision(TimePickerPrecision::Minute))
        .disabledTime([](TimeOfDay time, PickerEndpoint endpoint,
                         const TimePickerRangeValue& range) {
            return endpoint == PickerEndpoint::End && range.start && time < *range.start;
        })
        .onChange([value](TimePickerRangeValue next) mutable { value.set(next); }));
}
```

start 选择先发 `onCalendarChange`，再请求 End 激活；End 选择或 Confirm range 先验证整个范围，默认排序，然后发一次整体 onChange，最后请求关闭。`order=false` 保留端点顺序，应用可用禁用依赖限制。`policy.disabled` 独立保护两个端点，`allow_empty` 控制部分空值；完全空范围表示清空。`open`、`activeEndpoint` 可分别受控；`TimeRangePickerRef` 提供两个真实 InputRef。接受的端点激活会移动键盘焦点并选择输入文字；受控拒绝保留当前焦点，初始禁用 Start 时使用 End。关闭只恢复范围内的焦点，不抢占外部控件。拒绝整体回写后关闭恢复 accepted 值，pending 不泄漏为已保存值。

保留既有 clear、disabled、inputReadOnly、size、variant、status、autoFocus/ref 和键盘合同。方向键导航，Home/End 到首末可用值，Enter 确认，Escape 取消；Tab/Shift+Tab 保持原生焦点次序。子 popup 与编辑器 preedit 优先处理 Escape。Modal、隐藏、禁用、窗口失活取消捕获和 pending；普通关闭请求可以由受控应用拒绝。回调或谓词异常会退役失败的组件并清理输入／浮层资源。

显式 `FormField<String>` 适配保持有效：字段值通过 `parse_time_of_day` 转换，onChange 使用 `format_time_of_day` 写回，FormItem 通过 `.control(reference)` 连接真实 ref。即使 accepted 值未变，Form reset 也取消输入草稿、preedit 和 pending；不会伪造值变化。普通帮助、验证和 Theme 更新保留正在编辑的草稿。

Gallery 保留原有单值及 Form 样例，新增 12/24 小时、毫秒、Now、预设、滚轮选择和可拒绝的范围示例。平台通用 HEADLESS/Recording 证据与原生 GPU、Windows MSVC、系统 IME 候选 UI、辅助技术和跨显示器实测分开记录；Web DOM/CSS/React 属性不移植为原生空壳。合同基线固定为 [Ant Design 6.6.5](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/time-picker/index.en-US.md)。

当前 main 的 Linux 私有 Wayland/Vulkan 组合验收与剩余平台边界见 change107 的 `evidence/picker-integration.md`；隔离分支 HEADLESS 证据保持独立记录。
