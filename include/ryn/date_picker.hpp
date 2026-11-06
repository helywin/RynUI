#pragma once

#include <ryn/input.hpp>
#include <ryn/time_picker.hpp>
#include <compare>
#include <string_view>

namespace ryn {

// A date in the proleptic Gregorian calendar. Construction rejects invalid dates.
class CivilDate final {
public:
    CivilDate() = default;
    CivilDate(int year, unsigned month, unsigned day);

    [[nodiscard]] int year() const noexcept {
        return year_;
    }

    [[nodiscard]] unsigned month() const noexcept {
        return month_;
    }

    [[nodiscard]] unsigned day() const noexcept {
        return day_;
    }

    [[nodiscard]] static bool valid(int year, unsigned month, unsigned day) noexcept;
    friend auto operator<=>(const CivilDate&, const CivilDate&) = default;

private:
    int year_{1};
    unsigned month_{1};
    unsigned day_{1};
};

using DatePickerValue = std::optional<CivilDate>;
using DatePickerMultipleValue = std::vector<CivilDate>;
enum class DatePickerMode { Date, Week, Month, Quarter, Year, Decade };

struct CivilDateTime final {
    CivilDate date;
    TimeOfDay time;
    friend auto operator<=>(const CivilDateTime&, const CivilDateTime&) = default;
};

using DatePickerDateTimeValue = std::optional<CivilDateTime>;
[[nodiscard]] String format_date_time(CivilDateTime, std::string_view, const PickerLocale& = {});
[[nodiscard]] DatePickerDateTimeValue parse_date_time(std::string_view, std::string_view, const PickerLocale& = {});
void validate_date_time_format(std::string_view);

struct DatePickerDisabledInfo final {
    DatePickerMode type{DatePickerMode::Date};
    DatePickerValue from;
};

struct CalendarWeek final {
    int year{};
    unsigned week{};
    friend bool operator==(const CalendarWeek&, const CalendarWeek&) = default;
};

[[nodiscard]] CalendarWeek calendar_week(CivilDate, const PickerLocale& = {});
[[nodiscard]] String format_date(CivilDate, std::string_view, const PickerLocale& = {});
[[nodiscard]] DatePickerValue parse_date(std::string_view, std::string_view, const PickerLocale& = {});
void validate_date_format(std::string_view);

struct DatePickerCellInfo final {
    DatePickerValue date;
    DatePickerMode type{DatePickerMode::Date};
    bool selected{};
    bool disabled{};
    std::optional<PickerEndpoint> range;
    friend bool operator==(const DatePickerCellInfo&, const DatePickerCellInfo&) = default;
};

struct DatePickerCellSlot final {};

struct DatePickerPanelSlot final {};

struct DatePickerHeaderSlot final {};

using DatePickerCell = SlotContent<DatePickerCellSlot>;
using DatePickerPanel = SlotContent<DatePickerPanelSlot>;
using DatePickerHeader = SlotContent<DatePickerHeaderSlot>;
using DatePickerCellRenderer = std::function<DatePickerCell(Prop<DatePickerCellInfo>)>;

struct DatePickerPreset final {
    String key;
    String label;
    std::function<DatePickerValue()> value;
};

using DatePickerRef = InputRef;

// Predicate identity makes reactive replacement comparable without comparing closures.
class DisabledDate final {
public:
    DisabledDate() = default;

    template <typename F>
        requires std::is_invocable_r_v<bool, F, CivilDate>
    DisabledDate(F value) : function_(std::make_shared<const std::function<bool(CivilDate)>>(std::move(value))) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return bool(function_);
    }

    [[nodiscard]] bool operator()(CivilDate value) const {
        return (*function_)(value);
    }

    friend bool operator==(const DisabledDate&, const DisabledDate&) = default;

private:
    std::shared_ptr<const std::function<bool(CivilDate)>> function_;
};

[[nodiscard]] DatePickerValue parse_iso_date(std::string_view value);
[[nodiscard]] String format_iso_date(CivilDate value);

struct DatePickerMultiplePreset final {
    String key;
    String label;
    std::function<DatePickerMultipleValue()> value;
};

struct DatePickerDateTimePreset final {
    String key;
    String label;
    std::function<DatePickerDateTimeValue()> value;
};

namespace detail {
class DatePickerComponentHost;
}

class DatePickerProps final {
public:
    DatePickerProps& value(Prop<DatePickerValue> value) {
        value_ = std::move(value);
        return *this;
    }

    DatePickerProps& value(DatePickerValue value) {
        return this->value(Prop<DatePickerValue>{value});
    }

    DatePickerProps& value(CivilDate value) {
        return this->value(DatePickerValue{value});
    }

    DatePickerProps& defaultValue(DatePickerValue value) {
        default_value_ = value;
        return *this;
    }

    DatePickerProps& defaultPanelDate(CivilDate value) {
        panel_date_ = value;
        return *this;
    }

    DatePickerProps& minDate(Prop<DatePickerValue> value) {
        min_ = std::move(value);
        return *this;
    }

    DatePickerProps& minDate(CivilDate value) {
        return minDate(Prop<DatePickerValue>{DatePickerValue{value}});
    }

    DatePickerProps& maxDate(Prop<DatePickerValue> value) {
        max_ = std::move(value);
        return *this;
    }

    DatePickerProps& maxDate(CivilDate value) {
        return maxDate(Prop<DatePickerValue>{DatePickerValue{value}});
    }

    DatePickerProps& disabledDate(Prop<DisabledDate> value) {
        disabled_date_ = std::move(value);
        return *this;
    }

    template <typename F>
        requires std::is_invocable_r_v<bool, F, CivilDate>
    DatePickerProps& disabledDate(F value) {
        return disabledDate(Prop<DisabledDate>{DisabledDate{std::move(value)}});
    }

    DatePickerProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    DatePickerProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    DatePickerProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    DatePickerProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    DatePickerProps& inputReadOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    DatePickerProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    template <std::size_t N> DatePickerProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    DatePickerProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    DatePickerProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    DatePickerProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    DatePickerProps& ref(DatePickerRef value) {
        ref_ = std::move(value);
        return *this;
    }

    DatePickerProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    DatePickerProps& onChange(std::function<void(DatePickerValue)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    DatePickerProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    DatePickerProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    DatePickerProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    DatePickerProps& picker(Prop<DatePickerMode> v) {
        picker_ = std::move(v);
        return *this;
    }

    DatePickerProps& mode(Prop<DatePickerMode> v) {
        mode_ = std::move(v);
        return *this;
    }

    DatePickerProps& panelDate(Prop<CivilDate> v) {
        controlled_panel_ = std::move(v);
        return *this;
    }

    DatePickerProps& format(Prop<String> v) {
        format_ = std::move(v);
        return *this;
    }

    template <std::size_t N> DatePickerProps& format(const char8_t (&v)[N]) {
        return format(String{v});
    }

    DatePickerProps& inputFormats(Prop<std::vector<String>> v) {
        input_formats_ = std::move(v);
        return *this;
    }

    DatePickerProps& locale(Prop<PickerLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    DatePickerProps& direction(Prop<TextDirection> v) {
        direction_ = std::move(v);
        return *this;
    }

    DatePickerProps& placement(Prop<PickerPlacement> v) {
        placement_ = std::move(v);
        return *this;
    }

    DatePickerProps& needConfirm(Prop<bool> v = true) {
        need_confirm_ = std::move(v);
        return *this;
    }

    DatePickerProps& multiple(Prop<bool> v = true) {
        multiple_ = std::move(v);
        return *this;
    }

    DatePickerProps& multipleValue(Prop<DatePickerMultipleValue> v) {
        multiple_value_ = std::move(v);
        return *this;
    }

    DatePickerProps& defaultMultipleValue(DatePickerMultipleValue v) {
        default_multiple_ = std::move(v);
        return *this;
    }

    DatePickerProps& onMultipleChange(std::function<void(DatePickerMultipleValue)> v) {
        on_multiple_ = std::move(v);
        return *this;
    }

    DatePickerProps& showTime(TimePickerProps v = {}) {
        time_picker_ = std::move(v);
        return *this;
    }

    DatePickerProps& dateTimeValue(Prop<DatePickerDateTimeValue> v) {
        date_time_value_ = std::move(v);
        return *this;
    }

    DatePickerProps& defaultDateTimeValue(DatePickerDateTimeValue v) {
        default_date_time_ = v;
        return *this;
    }

    DatePickerProps& onDateTimeChange(std::function<void(DatePickerDateTimeValue)> v) {
        on_date_time_ = std::move(v);
        return *this;
    }

    DatePickerProps& disabledDateInfo(std::function<bool(CivilDate, const DatePickerDisabledInfo&)> v) {
        disabled_info_ = std::move(v);
        return *this;
    }

    DatePickerProps& disabledTime(std::function<bool(CivilDate, TimeOfDay)> v) {
        disabled_time_ = std::move(v);
        return *this;
    }

    DatePickerProps& clock(std::function<CivilDateTime()> v) {
        clock_ = std::move(v);
        return *this;
    }

    DatePickerProps& showNow(Prop<bool> v = true) {
        show_now_ = std::move(v);
        return *this;
    }

    DatePickerProps& presets(std::vector<DatePickerPreset> v) {
        presets_ = std::move(v);
        return *this;
    }

    DatePickerProps& multiplePresets(std::vector<DatePickerMultiplePreset> v) {
        multiple_presets_ = std::move(v);
        return *this;
    }

    DatePickerProps& dateTimePresets(std::vector<DatePickerDateTimePreset> v) {
        date_time_presets_ = std::move(v);
        return *this;
    }

    DatePickerProps& cellRender(DatePickerCellRenderer v) {
        cell_render_ = std::move(v);
        return *this;
    }

    DatePickerProps& panel(DatePickerPanel v) {
        panel_ = std::move(v);
        return *this;
    }

    DatePickerProps& header(DatePickerHeader v) {
        header_ = std::move(v);
        return *this;
    }

    DatePickerProps& suffixIcon(PickerIcon v) {
        suffix_icon_ = std::move(v);
        return *this;
    }

    DatePickerProps& onOk(std::function<void(DatePickerValue)> v) {
        on_ok_ = std::move(v);
        return *this;
    }

    DatePickerProps& onDateTimeOk(std::function<void(DatePickerDateTimeValue)> v) {
        on_time_ok_ = std::move(v);
        return *this;
    }

    DatePickerProps& onPanelChange(std::function<void(CivilDate, DatePickerMode)> v) {
        on_panel_ = std::move(v);
        return *this;
    }

    DatePickerProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DatePickerComponentHost;
    std::optional<Prop<DatePickerValue>> value_;
    std::optional<DatePickerValue> default_value_;
    DatePickerValue panel_date_;
    Prop<DatePickerValue> min_{DatePickerValue{}};
    Prop<DatePickerValue> max_{DatePickerValue{}};
    Prop<DisabledDate> disabled_date_{DisabledDate{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> allow_clear_{true};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    std::optional<Prop<String>> placeholder_;
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<DatePickerRef> ref_;
    bool auto_focus_{};
    std::function<void(DatePickerValue)> on_change_;
    std::function<detail::PickerRequestDisposition(DatePickerValue, const detail::PickerCandidate&)> on_request_;
    std::function<detail::PickerRequestDisposition(DatePickerDateTimeValue, const detail::PickerCandidate&)>
        on_time_request_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    Prop<DatePickerMode> picker_{DatePickerMode::Date};
    std::optional<Prop<DatePickerMode>> mode_;
    std::optional<Prop<CivilDate>> controlled_panel_;
    Prop<String> format_{String{}};
    Prop<std::vector<String>> input_formats_{std::vector<String>{}};
    std::optional<Prop<PickerLocale>> locale_;
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    Prop<PickerPlacement> placement_{PickerPlacement::BottomLeft};
    Prop<bool> multiple_{false};
    std::optional<Prop<bool>> need_confirm_;
    std::optional<Prop<DatePickerMultipleValue>> multiple_value_;
    std::optional<DatePickerMultipleValue> default_multiple_;
    std::optional<TimePickerProps> time_picker_;
    std::optional<Prop<DatePickerDateTimeValue>> date_time_value_;
    std::optional<DatePickerDateTimeValue> default_date_time_;
    Prop<bool> show_now_{false};
    std::function<CivilDateTime()> clock_;
    std::vector<DatePickerPreset> presets_;
    std::vector<DatePickerMultiplePreset> multiple_presets_;
    std::vector<DatePickerDateTimePreset> date_time_presets_;
    DatePickerCellRenderer cell_render_;
    std::optional<DatePickerPanel> panel_;
    std::optional<DatePickerHeader> header_;
    std::optional<PickerIcon> suffix_icon_;
    std::function<bool(CivilDate, const DatePickerDisabledInfo&)> disabled_info_;
    std::function<bool(CivilDate, TimeOfDay)> disabled_time_;
    std::function<void(DatePickerValue)> on_ok_;
    std::function<void(DatePickerDateTimeValue)> on_time_ok_;
    std::function<void(DatePickerMultipleValue)> on_multiple_;
    std::function<void(DatePickerDateTimeValue)> on_date_time_;
    std::function<void(CivilDate, DatePickerMode)> on_panel_;
    std::optional<PickerEndpoint> range_endpoint_;
    LayoutStyle layout_;
};

struct DatePickerFooterSlot final {};

using DatePickerFooter = SlotContent<DatePickerFooterSlot>;
void DatePicker(DatePickerProps props, std::optional<DatePickerFooter> footer = {});

using DatePickerRangeValue = PickerRange<CivilDate>;

struct DateRangePickerRef final {
    DatePickerRef start;
    DatePickerRef end;
};

class DateRangePickerProps final {
public:
    DateRangePickerProps& presets(std::vector<PickerRangePreset<CivilDate>> v) {
        presets_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& value(Prop<DatePickerRangeValue> v) {
        value_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& defaultValue(DatePickerRangeValue v) {
        default_value_ = v;
        return *this;
    }

    DateRangePickerProps& policy(Prop<PickerRangePolicy> v) {
        policy_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& defaultOpen(bool v = true) {
        default_open_ = v;
        return *this;
    }

    DateRangePickerProps& activeEndpoint(Prop<PickerEndpoint> v) {
        endpoint_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& picker(DatePickerProps v) {
        picker_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& ref(DateRangePickerRef v) {
        ref_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& disabledDate(std::function<bool(CivilDate, PickerEndpoint, const DatePickerRangeValue&)> v) {
        disabled_date_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& onChange(std::function<void(DatePickerRangeValue)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& onCalendarChange(std::function<void(DatePickerRangeValue, PickerEndpoint)> v) {
        on_calendar_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& onOpenChange(std::function<void(bool)> v) {
        on_open_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& onActiveEndpointChange(std::function<void(PickerEndpoint)> v) {
        on_endpoint_ = std::move(v);
        return *this;
    }

    DateRangePickerProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::DatePickerComponentHost;
    std::optional<Prop<DatePickerRangeValue>> value_;
    std::optional<DatePickerRangeValue> default_value_;
    Prop<PickerRangePolicy> policy_{PickerRangePolicy{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<Prop<PickerEndpoint>> endpoint_;
    Prop<bool> disabled_{false};
    DatePickerProps picker_;
    DateRangePickerRef ref_;
    std::function<bool(CivilDate, PickerEndpoint, const DatePickerRangeValue&)> disabled_date_;
    std::function<void(DatePickerRangeValue)> on_change_;
    std::function<void(DatePickerRangeValue, PickerEndpoint)> on_calendar_;
    std::function<void(bool)> on_open_;
    std::function<void(PickerEndpoint)> on_endpoint_;
    std::vector<PickerRangePreset<CivilDate>> presets_;
    LayoutStyle layout_;
};

using DateTimePickerRangeValue = PickerRange<CivilDateTime>;

class DateTimeRangePickerProps final {
public:
    DateTimeRangePickerProps& presets(std::vector<PickerRangePreset<CivilDateTime>> v) {
        presets_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& value(Prop<DateTimePickerRangeValue> v) {
        value_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& defaultValue(DateTimePickerRangeValue v) {
        default_value_ = v;
        return *this;
    }

    DateTimeRangePickerProps& policy(Prop<PickerRangePolicy> v) {
        policy_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& defaultOpen(bool v = true) {
        default_open_ = v;
        return *this;
    }

    DateTimeRangePickerProps& activeEndpoint(Prop<PickerEndpoint> v) {
        endpoint_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& picker(DatePickerProps v) {
        picker_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& ref(DateRangePickerRef v) {
        ref_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps&
    disabledDateTime(std::function<bool(CivilDateTime, PickerEndpoint, const DateTimePickerRangeValue&)> v) {
        disabled_date_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& onChange(std::function<void(DateTimePickerRangeValue)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& onCalendarChange(std::function<void(DateTimePickerRangeValue, PickerEndpoint)> v) {
        on_calendar_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& onOpenChange(std::function<void(bool)> v) {
        on_open_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& onActiveEndpointChange(std::function<void(PickerEndpoint)> v) {
        on_endpoint_ = std::move(v);
        return *this;
    }

    DateTimeRangePickerProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::DatePickerComponentHost;
    std::optional<Prop<DateTimePickerRangeValue>> value_;
    std::optional<DateTimePickerRangeValue> default_value_;
    Prop<PickerRangePolicy> policy_{PickerRangePolicy{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<Prop<PickerEndpoint>> endpoint_;
    Prop<bool> disabled_{false};
    DatePickerProps picker_;
    DateRangePickerRef ref_;
    std::function<bool(CivilDateTime, PickerEndpoint, const DateTimePickerRangeValue&)> disabled_date_;
    std::function<void(DateTimePickerRangeValue)> on_change_;
    std::function<void(DateTimePickerRangeValue, PickerEndpoint)> on_calendar_;
    std::function<void(bool)> on_open_;
    std::function<void(PickerEndpoint)> on_endpoint_;
    std::vector<PickerRangePreset<CivilDateTime>> presets_;
    LayoutStyle layout_;
};

void DateRangePicker(DateRangePickerProps props = {}, std::optional<DatePickerFooter> footer = {});
void DateRangePicker(DateTimeRangePickerProps props, std::optional<DatePickerFooter> footer = {});

} // namespace ryn
