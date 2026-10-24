#pragma once

#include <ryn/input.hpp>
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
using DatePickerRef = InputRef;
using DisabledDate = std::function<bool(CivilDate)>;
[[nodiscard]] DatePickerValue parse_iso_date(std::string_view value);
[[nodiscard]] String format_iso_date(CivilDate value);

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
    Prop<String> placeholder_{String{u8"Select date"}};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<DatePickerRef> ref_;
    bool auto_focus_{};
    std::function<void(DatePickerValue)> on_change_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    LayoutStyle layout_;
};

struct DatePickerFooterSlot final {};

using DatePickerFooter = SlotContent<DatePickerFooterSlot>;
void DatePicker(DatePickerProps props, std::optional<DatePickerFooter> footer = {});

} // namespace ryn
