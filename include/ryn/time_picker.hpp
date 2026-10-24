#pragma once

#include <ryn/input.hpp>

#include <compare>
#include <stdexcept>
#include <string_view>

namespace ryn {

// A valid time within one day. There is deliberately no date, zone or clock.
class TimeOfDay final {
public:
    constexpr TimeOfDay(int hour = 0, int minute = 0, int second = 0) {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
            throw std::invalid_argument("TimeOfDay requires hour 0..23 and minute/second 0..59");
        }
        seconds_ = hour * 3600 + minute * 60 + second;
    }

    [[nodiscard]] constexpr int hour() const noexcept {
        return seconds_ / 3600;
    }

    [[nodiscard]] constexpr int minute() const noexcept {
        return seconds_ / 60 % 60;
    }

    [[nodiscard]] constexpr int second() const noexcept {
        return seconds_ % 60;
    }

    friend auto operator<=>(const TimeOfDay&, const TimeOfDay&) = default;

private:
    int seconds_{};
};

enum class TimePickerPrecision { Second, Minute };
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
};

[[nodiscard]] std::optional<TimeOfDay> parse_time_of_day(std::string_view,
                                                         TimePickerPrecision = TimePickerPrecision::Second);
[[nodiscard]] String format_time_of_day(TimeOfDay, TimePickerPrecision = TimePickerPrecision::Second);

namespace detail {
class TimePickerComponentHost;
}

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

    TimePickerProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TimePickerComponentHost;
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
    std::function<void(bool)> on_open_;
    LayoutStyle layout_;
};

struct TimePickerFooterSlot final {};

using TimePickerFooter = SlotContent<TimePickerFooterSlot>;
void TimePicker(TimePickerProps props = {}, std::optional<TimePickerFooter> footer = {});

} // namespace ryn
