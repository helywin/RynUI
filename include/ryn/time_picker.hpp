#pragma once

#include <ryn/input.hpp>
#include <ryn/picker_common.hpp>

#include <compare>
#include <stdexcept>
#include <string_view>

namespace ryn {

// A valid time within one day. There is deliberately no date, zone or clock.
class TimeOfDay final {
public:
    constexpr TimeOfDay(int hour = 0, int minute = 0, int second = 0, int millisecond = 0) {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59 || millisecond < 0 ||
            millisecond > 999) {
            throw std::invalid_argument("TimeOfDay requires hour 0..23 and minute/second 0..59");
        }
        milliseconds_ = (hour * 3600 + minute * 60 + second) * 1000 + millisecond;
    }

    [[nodiscard]] constexpr int hour() const noexcept {
        return milliseconds_ / 3600000;
    }

    [[nodiscard]] constexpr int minute() const noexcept {
        return milliseconds_ / 60000 % 60;
    }

    [[nodiscard]] constexpr int second() const noexcept {
        return milliseconds_ / 1000 % 60;
    }

    [[nodiscard]] constexpr int millisecond() const noexcept {
        return milliseconds_ % 1000;
    }

    friend auto operator<=>(const TimeOfDay&, const TimeOfDay&) = default;

private:
    int milliseconds_{};
};

enum class TimePickerPrecision { Second, Minute, Millisecond };
enum class TimePickerColumn { Hour, Minute, Second, Millisecond, Meridiem };
using TimePickerValue = std::optional<TimeOfDay>;
using TimePickerRef = InputRef;
using TimePickerDisabledTime = std::function<bool(TimeOfDay)>;

// Inclusive, non-wrapping range; steps are anchored at zero in each column.
struct TimePickerRules final {
    int hour_step{1};
    int minute_step{1};
    int second_step{1};
    TimePickerValue min;
    TimePickerValue max;
    TimePickerDisabledTime disabled;
    int millisecond_step{1};
    std::function<bool(int)> disabled_hours;
    std::function<bool(int, int)> disabled_minutes;
    std::function<bool(int, int, int)> disabled_seconds;
    std::function<bool(int, int, int, int)> disabled_milliseconds;

    // Predicates cannot be compared: conservatively refresh when either is present.
    friend bool operator==(const TimePickerRules& a, const TimePickerRules& b) {
        return a.hour_step == b.hour_step && a.minute_step == b.minute_step && a.second_step == b.second_step &&
               a.millisecond_step == b.millisecond_step && a.min == b.min && a.max == b.max && !a.disabled &&
               !b.disabled && !a.disabled_hours && !b.disabled_hours && !a.disabled_minutes && !b.disabled_minutes &&
               !a.disabled_seconds && !b.disabled_seconds && !a.disabled_milliseconds && !b.disabled_milliseconds;
    }
};

[[nodiscard]] std::optional<TimeOfDay> parse_time_of_day(std::string_view,
                                                         TimePickerPrecision = TimePickerPrecision::Second);
[[nodiscard]] String format_time_of_day(TimeOfDay, TimePickerPrecision = TimePickerPrecision::Second);

[[nodiscard]] String format_time_of_day(TimeOfDay, std::string_view format, const PickerLocale& = {});
[[nodiscard]] TimePickerValue parse_time_of_day(std::string_view, std::string_view format, const PickerLocale& = {});
void validate_time_format(std::string_view);

struct TimePickerCellInfo final {
    TimePickerColumn column{TimePickerColumn::Hour};
    int value{};
    bool selected{};
    bool disabled{};
    std::optional<PickerEndpoint> range;
    friend bool operator==(const TimePickerCellInfo&, const TimePickerCellInfo&) = default;
};

struct TimePickerCellSlot final {};

struct TimePickerPanelSlot final {};

using TimePickerCell = SlotContent<TimePickerCellSlot>;
using TimePickerPanel = SlotContent<TimePickerPanelSlot>;
using TimePickerCellRenderer = std::function<TimePickerCell(Prop<TimePickerCellInfo>)>;

struct TimePickerPreset final {
    String key;
    String label;
    std::function<TimePickerValue()> value;
};

namespace detail {
class TimePickerComponentHost;
class DatePickerComponentHost;
} // namespace detail

class TimePickerProps final {
public:
    TimePickerProps& value(Prop<TimePickerValue> value) {
        value_ = std::move(value);
        return *this;
    }

    TimePickerProps& value(TimePickerValue value) {
        return this->value(Prop<TimePickerValue>{value});
    }

    TimePickerProps& value(TimeOfDay value) {
        return this->value(TimePickerValue{value});
    }

    TimePickerProps& defaultValue(TimePickerValue value) {
        default_value_ = value;
        return *this;
    }

    TimePickerProps& defaultPanelValue(TimeOfDay value) {
        panel_value_ = value;
        return *this;
    }

    TimePickerProps& precision(Prop<TimePickerPrecision> value) {
        precision_ = std::move(value);
        return *this;
    }

    TimePickerProps& rules(Prop<TimePickerRules> value) {
        rules_ = std::move(value);
        return *this;
    }

    TimePickerProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    TimePickerProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    TimePickerProps& needConfirm(Prop<bool> value = true) {
        need_confirm_ = std::move(value);
        return *this;
    }

    TimePickerProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    TimePickerProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TimePickerProps& inputReadOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    TimePickerProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    TimePickerProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    TimePickerProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    TimePickerProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TimePickerProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    TimePickerProps& ref(TimePickerRef value) {
        ref_ = std::move(value);
        return *this;
    }

    TimePickerProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    TimePickerProps& onChange(std::function<void(TimePickerValue)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TimePickerProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    TimePickerProps& use12Hours(Prop<bool> v = true) {
        use_12_hours_ = std::move(v);
        return *this;
    }

    TimePickerProps& format(Prop<String> v) {
        format_ = std::move(v);
        return *this;
    }

    template <std::size_t N> TimePickerProps& format(const char8_t (&v)[N]) {
        return format(String{v});
    }

    TimePickerProps& locale(Prop<PickerLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    TimePickerProps& changeOnScroll(Prop<bool> v = true) {
        change_on_scroll_ = std::move(v);
        return *this;
    }

    TimePickerProps& clock(std::function<TimeOfDay()> v) {
        clock_ = std::move(v);
        return *this;
    }

    TimePickerProps& showNow(Prop<bool> v = true) {
        show_now_ = std::move(v);
        return *this;
    }

    TimePickerProps& presets(std::vector<TimePickerPreset> v) {
        presets_ = std::move(v);
        return *this;
    }

    TimePickerProps& direction(Prop<TextDirection> v) {
        direction_ = std::move(v);
        return *this;
    }

    TimePickerProps& placement(Prop<PickerPlacement> v) {
        placement_ = std::move(v);
        return *this;
    }

    TimePickerProps& cellRender(TimePickerCellRenderer v) {
        cell_render_ = std::move(v);
        return *this;
    }

    TimePickerProps& panel(TimePickerPanel v) {
        panel_ = std::move(v);
        return *this;
    }

    TimePickerProps& suffixIcon(PickerIcon v) {
        suffix_icon_ = std::move(v);
        return *this;
    }

    TimePickerProps& onSelect(std::function<void(TimeOfDay)> v) {
        on_select_ = std::move(v);
        return *this;
    }

    TimePickerProps& onOk(std::function<void(TimePickerValue)> v) {
        on_ok_ = std::move(v);
        return *this;
    }

    TimePickerProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TimePickerComponentHost;
    friend class detail::DatePickerComponentHost;
    std::optional<Prop<TimePickerValue>> value_;
    std::optional<TimePickerValue> default_value_;
    TimeOfDay panel_value_;
    Prop<TimePickerPrecision> precision_{TimePickerPrecision::Second};
    Prop<TimePickerRules> rules_{TimePickerRules{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> need_confirm_{true};
    Prop<bool> allow_clear_{true};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<String> placeholder_{String{u8"Select a time"}};
    std::optional<TimePickerRef> ref_;
    bool auto_focus_{};
    std::function<void(TimePickerValue)> on_change_;
    std::function<void(TimePickerValue)> on_request_;
    std::function<void(bool)> on_open_;
    Prop<bool> use_12_hours_{false};
    Prop<String> format_{String{}};
    Prop<PickerLocale> locale_{PickerLocale{}};
    Prop<bool> change_on_scroll_{false};
    Prop<bool> show_now_{false};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    Prop<PickerPlacement> placement_{PickerPlacement::BottomLeft};
    std::function<TimeOfDay()> clock_;
    std::vector<TimePickerPreset> presets_;
    TimePickerCellRenderer cell_render_;
    std::optional<TimePickerPanel> panel_;
    std::optional<PickerIcon> suffix_icon_;
    std::function<void(TimeOfDay)> on_select_;
    std::function<void(TimePickerValue)> on_ok_;
    std::optional<PickerEndpoint> range_endpoint_;
    LayoutStyle layout_;
};

struct TimePickerFooterSlot final {};

using TimePickerFooter = SlotContent<TimePickerFooterSlot>;
void TimePicker(TimePickerProps props = {}, std::optional<TimePickerFooter> footer = {});

using TimePickerRangeValue = PickerRange<TimeOfDay>;

struct TimeRangePickerRef final {
    TimePickerRef start;
    TimePickerRef end;
};

class TimeRangePickerProps final {
public:
    TimeRangePickerProps& presets(std::vector<PickerRangePreset<TimeOfDay>> v) {
        presets_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& value(Prop<TimePickerRangeValue> v) {
        value_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& defaultValue(TimePickerRangeValue v) {
        default_value_ = v;
        return *this;
    }

    TimeRangePickerProps& policy(Prop<PickerRangePolicy> v) {
        policy_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& defaultOpen(bool v = true) {
        default_open_ = v;
        return *this;
    }

    TimeRangePickerProps& activeEndpoint(Prop<PickerEndpoint> v) {
        endpoint_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& picker(TimePickerProps v) {
        picker_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& ref(TimeRangePickerRef v) {
        ref_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& disabledTime(std::function<bool(TimeOfDay, PickerEndpoint, const TimePickerRangeValue&)> v) {
        disabled_time_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& onChange(std::function<void(TimePickerRangeValue)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& onCalendarChange(std::function<void(TimePickerRangeValue, PickerEndpoint)> v) {
        on_calendar_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& onOpenChange(std::function<void(bool)> v) {
        on_open_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& onActiveEndpointChange(std::function<void(PickerEndpoint)> v) {
        on_endpoint_ = std::move(v);
        return *this;
    }

    TimeRangePickerProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::TimePickerComponentHost;
    friend class detail::DatePickerComponentHost;
    std::optional<Prop<TimePickerRangeValue>> value_;
    std::optional<TimePickerRangeValue> default_value_;
    Prop<PickerRangePolicy> policy_{PickerRangePolicy{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<Prop<PickerEndpoint>> endpoint_;
    Prop<bool> disabled_{false};
    TimePickerProps picker_;
    TimeRangePickerRef ref_;
    std::function<bool(TimeOfDay, PickerEndpoint, const TimePickerRangeValue&)> disabled_time_;
    std::function<void(TimePickerRangeValue)> on_change_;
    std::function<void(TimePickerRangeValue, PickerEndpoint)> on_calendar_;
    std::function<void(bool)> on_open_;
    std::function<void(PickerEndpoint)> on_endpoint_;
    std::vector<PickerRangePreset<TimeOfDay>> presets_;
    LayoutStyle layout_;
};

void TimeRangePicker(TimeRangePickerProps props = {}, std::optional<TimePickerFooter> footer = {});

} // namespace ryn
