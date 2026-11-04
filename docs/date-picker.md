# DatePicker

`CivilDate` 仍表示 0001-01-01 至 9999-12-31 的有效 Gregorian 公历日期，没有 epoch、时间或时区含义。原有 `DatePickerValue = std::optional<CivilDate>`、严格 ISO 转换及单值调用继续有效。

```cpp
#include <ryn/rynui.hpp>

void date_field(ryn::Signal<ryn::DatePickerValue> selected) {
    using namespace ryn;
    DatePicker(DatePickerProps{}.value(selected)
        .defaultPanelDate(CivilDate{2026, 10, 1})
        .minDate(CivilDate{2020, 1, 1}).maxDate(CivilDate{2030, 12, 31})
        .onChange([selected](DatePickerValue date) mutable { selected.set(date); }),
        DatePickerFooter{[] { Text(u8"Choose a date"); }});
}
```

`value/defaultValue`、`open/defaultOpen` 分别互斥。受控 onChange/onOpenChange 是请求，父级不回写时保留外部 authority。minDate/maxDate 限制浏览及提交，周／月／季度／年份按所选期间与范围的交集判断；disabledDate 判断日期资格，`disabledDateInfo` 还接收 picker 类型和范围的 from 日期。非法范围在发布前拒绝，不扫描寻找全局可选日期，也不静默改写外部选择。

`picker` 支持 Date、Week、Month、Quarter、Year，实际切换日格、月格、季度或年份面板。`mode` 另外控制当前浏览面板（含 Decade），标题可上钻，选中年份／月份向目标模式下钻。非日模式的头部按年／十年／百年导航，键盘高亮使用相同的日期期间。`panelDate` 和 mode 均可受控，通过 `onPanelChange(date, mode)` 请求变化。普通 Prop 与 Theme 更新保留单元格及 footer 内容，不重建其他控件。

`PickerLocale` 提供 months、short_months、星期名、first_day（Sunday=0）、first_week_contains、显示纪年偏移和动作文字。ISO 周配置为 first_day=1、first_week_contains=4；`calendar_week` 正确处理跨年周。纪年偏移只作用于显示／解析，不改变 CivilDate 的 Gregorian 含义。无值面板保留旧的 UTC 当前日期回退；业务本地日期通过 defaultPanelDate 或显式 `.clock([] { return CivilDateTime{...}; })` 提供。Today/Now 必须有显式时钟。

`format` 和 `inputFormats` 接受验证后的格式，首个格式用于显示，附加格式用于输入。支持 YYYY、M/MM/MMM/MMMM、D/DD、Q、gggg、w/ww 及 `[literal]`；周格式使用 `gggg-[W]ww`。`parse_date/format_date` 共用同一规则，`parse_iso_date/format_iso_date` 保持原有十字节 ISO 合同。非法或不完整草稿不会发布，preedit 不提交；失焦恢复 accepted 显示。

```cpp
void week_field(ryn::Signal<ryn::DatePickerValue> selected) {
    using namespace ryn;
    PickerLocale iso;
    iso.first_day = 1;
    iso.first_week_contains = 4;
    DatePicker(DatePickerProps{}.value(selected).picker(DatePickerMode::Week).locale(iso)
        .format(u8"gggg-[W]ww").defaultPanelDate(CivilDate{2026, 11, 1})
        .onChange([selected](DatePickerValue next) mutable { selected.set(next); }));
}
```

`multiple(true)` 使用独立的 `DatePickerMultipleValue`、multipleValue/defaultMultipleValue 和 onMultipleChange。点击切换 pending 中的日期，默认 OK 才请求整个有序集合；受控拒绝及取消恢复 accepted 集合。多选不能与 showTime 组合，避免不明确的日期时间集合语义。

`CivilDateTime` 组合 CivilDate 和整数精度 TimeOfDay。showTime 接受共用 TimePickerProps，提供保留时间选择器；dateTimeValue/defaultDateTimeValue 和 onDateTimeChange 原子传递组合值。日期选择先进入 pending；OK 同时验证日期、时间步长与 disabledTime，才请求组合值。日期时间格式支持日期格式加时间格式，例如 `DD/MM/YYYY HH:mm:ss.SSS`；`parse_date_time/format_date_time` 也公开可用。

```cpp
void date_time_field(ryn::Signal<ryn::DatePickerDateTimeValue> value) {
    using namespace ryn;
    DatePicker(DatePickerProps{}.showTime(TimePickerProps{}.precision(TimePickerPrecision::Millisecond))
        .dateTimeValue(value).format(u8"DD/MM/YYYY HH:mm:ss.SSS")
        .disabledTime([](CivilDate date, TimeOfDay time) {
            return date.day() == 1 && time.hour() < 8;
        })
        .onDateTimeChange([value](DatePickerDateTimeValue next) mutable { value.set(next); }));
}
```

日期范围命名为 `DateRangePicker`，对应上游 DatePicker.RangePicker；时间范围另名 TimeRangePicker。DateRangePickerProps 使用 DatePickerRangeValue 的独立可空 start/end，`picker(DatePickerProps)` 配置共用外观、格式和规则。PickerRangePolicy 控制部分空端点、端点禁用和默认排序；order=false 保留原顺序，整体空范围表示清空。start 更新先发 onCalendarChange，再激活 End；End 或 Confirm range 原子验证并发整体 onChange，然后请求关闭。open 和 activeEndpoint 可受控，DateRangePickerRef 提供两个真实 InputRef。激活变化同时把键盘焦点和输入选择移到实际接受的端点；受控拒绝不会移动焦点，初始 Start 被禁用时使用 End。关闭只恢复范围内的焦点，不抢占外部控件。

```cpp
void date_range(ryn::Signal<ryn::DatePickerRangeValue> value) {
    using namespace ryn;
    PickerRangePolicy policy;
    policy.allow_empty = {true, false};
    DateRangePicker(DateRangePickerProps{}.value(value).policy(policy)
        .picker(DatePickerProps{}.format(u8"DD/MM/YYYY").defaultPanelDate(CivilDate{2026, 11, 1}))
        .disabledDate([](CivilDate date, PickerEndpoint endpoint, const DatePickerRangeValue& range) {
            return endpoint == PickerEndpoint::End && range.start && date < *range.start;
        })
        .onChange([value](DatePickerRangeValue next) mutable { value.set(next); }));
}
```

日期时间范围使用 `DateRangePicker(DateTimeRangePickerProps)` 重载，值为 `DateTimePickerRangeValue = PickerRange<CivilDateTime>`，使用同样的范围策略和事件。该重载自动包含时间面板，`.picker(DatePickerProps{}.showTime(...))` 可进一步配置精度。普通日期范围不能混用 showTime，避免把时间从公开值中丢弃。

presets 由唯一 key、标签和点击时求值的回调组成，multiplePresets/dateTimePresets 分别传递整个日期集合和精确 CivilDateTime；clock 提供 Today/Now。`cellRender(Prop<DatePickerCellInfo>)` 返回只构造一次的 typed DatePickerCell；日期／选择／禁用和范围端点信息通过 Prop 更新；首次构造也提供实际单元格数据。panel、header、suffixIcon 及 DatePickerFooter 提供保留内容入口。自定义面板可通过应用 Signal 和公开回调组织选择，富 Input 草稿不会因月份或 Theme 更新丢失。

保留 allowClear、disabled、inputReadOnly、ref、autoFocus、size、variant、status、onFocus/onBlur。LayoutStyle 控制外部布局，Theme/Component Token 控制视觉。RTL 反转网格及左右键；placement 四角定位并在窗口内回退，窄窗口限制宽度。日模式方向键按日／周，PageUp/PageDown 按月，Shift 按年；其他模式按对应单元导航。Tab/Shift+Tab 保持原生焦点，内层 popup 和 preedit 优先处理 Escape。开合使用有限 NavigationTransition，reduced motion、窗口失活、隐藏或禁用终止工作。

显式 FormField<String> 适配继续使用 ISO 转换与真实 ref：

```cpp
void form_date_field(const ryn::FormRef& form, ryn::FormField<ryn::String> field,
                     const ryn::DatePickerRef& picker) {
    using namespace ryn;
    Form(FormProps{}.ref(form), FormContent{[field, picker] {
        FormItem(FormItemProps{}.field(String{u8"date"}, field).control(picker),
            FormItemContent{[field, picker] {
                DatePicker(DatePickerProps{}
                    .value(bind([field] { return parse_iso_date(field.get().bytes()); }))
                    .defaultPanelDate(CivilDate{2026, 10, 1}).status(field.status()).ref(picker)
                    .onChange([change = field.onChange()](DatePickerValue date) {
                        change(date ? format_iso_date(*date) : String{});
                    }));
            }});
    }});
}
```

即使 accepted 值与初值相同，显式 Form reset 也取消 preedit、输入草稿和 pending，恢复 accepted 值，不发布伪变化。普通验证、帮助、status 和 Theme 更新保留草稿。禁用、隐藏、Modal/Spin 或窗口失活 suppress 浮层并清理捕获／焦点，恢复后需要显式打开；回调、谓词或 renderer 异常退役失败内容。

Gallery 保留原有 ISO 日期和 Form 样例，新增实际模式、周起点、RTL、Today/presets、多选／范围受控拒绝及日期时间组合示例。HEADLESS/Recording 不替代原生 GPU、Windows MSVC、系统 IME 候选 UI、辅助技术或跨显示器实测。Web DOM/CSS/React 属性不作为原生空壳移植。合同基线固定为 [Ant Design 6.6.5](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/date-picker/index.en-US.md)。

当前 main 的 Linux 私有 Wayland/Vulkan 组合验收与剩余平台边界见 change107 的 `evidence/picker-integration.md`；隔离分支 HEADLESS 证据保持独立记录。
