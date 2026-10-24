# DatePicker

首版为保留的原生单日期控件。`CivilDate` 表示 0001-01-01 到 9999-12-31 的有效 Gregorian 公历日期，构造非法日期抛出 `std::invalid_argument`。日期没有时区、时间或 epoch 含义。`DatePickerValue` 是 `std::optional<CivilDate>`；空值表示未选择。

`parse_iso_date(std::string_view)` 只接受十字节 `YYYY-MM-DD`，非法或空字符串返回 `nullopt`。`format_iso_date(CivilDate)` 总是返回同一固定格式。编辑中的空字符串可提交为清空，其他非法或禁用草稿不发布，失焦恢复当前选择。IME preedit 不作为日期发布。

```cpp
#include <ryn/rynui.hpp>

void date_field(ryn::Signal<ryn::DatePickerValue>& selected) {
    using namespace ryn;
    DatePicker(DatePickerProps{}
        .value(selected)
        .defaultPanelDate(CivilDate{2026, 10, 1})
        .minDate(CivilDate{2020, 1, 1})
        .maxDate(CivilDate{2030, 12, 31})
        .onChange([&selected](DatePickerValue date) { selected.set(date); }),
        DatePickerFooter{[] { Text(u8"Choose a date"); }});
}
```

`value` 与 `defaultValue`、`open` 与 `defaultOpen` 分别互斥。受控回调是请求，调用方不回写则保留原值。范围改变不会改写受控选择。`minDate`、`maxDate` 限制浏览及提交，`disabledDate` 只控制提交资格；禁用所有日期也不扫描寻找下一可用日期。`minDate > maxDate` 抛出异常。禁用规则应为无副作用的轻量谓词。

面板初始日期依次来自选择值、`defaultPanelDate`、运行时 **UTC 当前日期**。这里不提供本地时区“今天”服务；需要业务本地月份时由调用方提供 `defaultPanelDate`。面板保留浏览状态，不随每次打开重置。

支持 `allowClear`（默认 true）、`disabled`、`inputReadOnly`、`placeholder`、三种 `ControlSize`、四种 `InputVariant` 和三种 `InputStatus`、`DatePickerRef`（InputRef）、`autoFocus`、`onFocus/onBlur`、受控开合及 retained typed `DatePickerFooter`。`inputReadOnly` 只禁用手工编辑，仍可选择和清空。

月历固定六周、每周从周日开始。方向键按日/周移动，PageUp/PageDown 按月，Shift+PageUp/PageDown 按年，Home/End 到本月首末日，Enter 选择；关闭时 Down 打开。输入草稿 Enter 提交。Escape 关闭，Tab 正常离开，不创建 42 个 Tab stop。月历标题及工作日为固定数字/英文，首版没有 locale 配置。

首版不提供 RangePicker、多选、时间、周/月/季/年 picker、自由格式、locale/RTL、时区转换、预设、自定义 cell/panel、动画或辅助技术桥接；不声称完整覆盖 Ant Design DatePicker。Gallery、GPU、原生窗口/DPI、Linux 和系统 IME 的实际验收独立于本 change 的 Windows MSVC headless 合同测试。
