#pragma once
#include <ryn/date_picker.hpp>

namespace ryn {
enum class CalendarMode { Month, Year };
enum class CalendarSelectSource { Date, Month, Year, Customize };

struct CalendarSelectInfo final {
    CalendarSelectSource source{CalendarSelectSource::Date};
    friend bool operator==(const CalendarSelectInfo&, const CalendarSelectInfo&) = default;
};

struct CalendarRange final {
    CivilDate start;
    CivilDate end;
    friend bool operator==(const CalendarRange&, const CalendarRange&) = default;
};

struct CalendarCellInfo final {
    CivilDate date;
    CalendarMode mode{CalendarMode::Month};
    bool selected{};
    bool today{};
    bool disabled{};
    bool in_view{};
    CalendarWeek week;
    friend bool operator==(const CalendarCellInfo&, const CalendarCellInfo&) = default;
};

struct CalendarHeaderInfo final {
    CivilDate value;
    CalendarMode mode{CalendarMode::Month};
    bool fullscreen{true};
    PickerLocale locale;
    friend bool operator==(const CalendarHeaderInfo&, const CalendarHeaderInfo&) = default;
};

namespace detail {
class CalendarComponentHost;
struct CalendarRefState;
} // namespace detail

class CalendarRef final {
public:
    CalendarRef();
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool select(CivilDate, CalendarSelectSource = CalendarSelectSource::Customize) const;
    [[nodiscard]] bool changeMode(CalendarMode) const;

private:
    friend class detail::CalendarComponentHost;
    std::shared_ptr<detail::CalendarRefState> state_;
};

struct CalendarCellSlot final {};

struct CalendarHeaderSlot final {};

using CalendarCell = SlotContent<CalendarCellSlot>;
using CalendarHeader = SlotContent<CalendarHeaderSlot>;

// A comparable factory identity. Metadata is reactive; the factory runs only on
// first mount or explicit replacement, never on ordinary date/state updates.
class CalendarCellRenderer final {
public:
    CalendarCellRenderer() = default;

    template <typename F>
        requires std::is_invocable_r_v<CalendarCell, F, Binding<CalendarCellInfo>>
    CalendarCellRenderer(F f)
        : function_(std::make_shared<const std::function<CalendarCell(Binding<CalendarCellInfo>)>>(std::move(f))) {}

    explicit operator bool() const noexcept {
        return bool(function_);
    }

    CalendarCell operator()(Binding<CalendarCellInfo> info) const {
        return (*function_)(std::move(info));
    }

    friend bool operator==(const CalendarCellRenderer&, const CalendarCellRenderer&) = default;

private:
    std::shared_ptr<const std::function<CalendarCell(Binding<CalendarCellInfo>)>> function_;
};

struct CalendarHeaderContext final {
    Binding<CalendarHeaderInfo> info;
    CalendarRef calendar;
};

class CalendarHeaderRenderer final {
public:
    CalendarHeaderRenderer() = default;

    template <typename F>
        requires std::is_invocable_r_v<CalendarHeader, F, CalendarHeaderContext>
    CalendarHeaderRenderer(F f)
        : function_(std::make_shared<const std::function<CalendarHeader(CalendarHeaderContext)>>(std::move(f))) {}

    explicit operator bool() const noexcept {
        return bool(function_);
    }

    CalendarHeader operator()(CalendarHeaderContext context) const {
        return (*function_)(std::move(context));
    }

    friend bool operator==(const CalendarHeaderRenderer&, const CalendarHeaderRenderer&) = default;

private:
    std::shared_ptr<const std::function<CalendarHeader(CalendarHeaderContext)>> function_;
};

class CalendarProps final {
public:
    CalendarProps& value(Prop<CivilDate> v) {
        value_ = std::move(v);
        return *this;
    }

    CalendarProps& defaultValue(CivilDate v) {
        default_value_ = v;
        return *this;
    }

    CalendarProps& mode(Prop<CalendarMode> v) {
        mode_ = std::move(v);
        return *this;
    }

    CalendarProps& defaultMode(CalendarMode v) {
        default_mode_ = v;
        return *this;
    }

    CalendarProps& fullscreen(Prop<bool> v) {
        fullscreen_ = std::move(v);
        return *this;
    }

    CalendarProps& showWeek(Prop<bool> v = true) {
        show_week_ = std::move(v);
        return *this;
    }

    CalendarProps& validRange(Prop<std::optional<CalendarRange>> v) {
        range_ = std::move(v);
        return *this;
    }

    CalendarProps& validRange(CalendarRange v) {
        return validRange(Prop<std::optional<CalendarRange>>{std::optional{v}});
    }

    CalendarProps& disabledDate(Prop<DisabledDate> v) {
        disabled_date_ = std::move(v);
        return *this;
    }

    template <typename F>
        requires std::is_invocable_r_v<bool, F, CivilDate>
    CalendarProps& disabledDate(F f) {
        return disabledDate(Prop<DisabledDate>{DisabledDate{std::move(f)}});
    }

    CalendarProps& locale(Prop<PickerLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    CalendarProps& direction(Prop<TextDirection> v) {
        direction_ = std::move(v);
        return *this;
    }

    CalendarProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    CalendarProps& visible(Prop<bool> v = true) {
        visible_ = std::move(v);
        return *this;
    }

    CalendarProps& ref(CalendarRef v) {
        ref_ = std::move(v);
        return *this;
    }

    CalendarProps& clock(std::function<CivilDate()> v) {
        clock_ = std::move(v);
        return *this;
    }

    CalendarProps& showToday(Prop<bool> v = true) {
        show_today_ = std::move(v);
        return *this;
    }

    CalendarProps& cellRender(Prop<CalendarCellRenderer> v) {
        cell_ = std::move(v);
        return *this;
    }

    CalendarProps& fullCellRender(Prop<CalendarCellRenderer> v) {
        full_cell_ = std::move(v);
        return *this;
    }

    CalendarProps& dateCellRender(Prop<CalendarCellRenderer> v) {
        date_cell_ = std::move(v);
        return *this;
    }

    CalendarProps& monthCellRender(Prop<CalendarCellRenderer> v) {
        month_cell_ = std::move(v);
        return *this;
    }

    CalendarProps& headerRender(Prop<CalendarHeaderRenderer> v) {
        header_ = std::move(v);
        return *this;
    }

    CalendarProps& headerRender(CalendarHeaderRenderer v) {
        return headerRender(Prop<CalendarHeaderRenderer>{std::move(v)});
    }

    CalendarProps& onChange(std::function<void(CivilDate)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    CalendarProps& onSelect(std::function<void(CivilDate, CalendarSelectInfo)> v) {
        on_select_ = std::move(v);
        return *this;
    }

    CalendarProps& onPanelChange(std::function<void(CivilDate, CalendarMode)> v) {
        on_panel_ = std::move(v);
        return *this;
    }

    CalendarProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::CalendarComponentHost;
    std::optional<Prop<CivilDate>> value_;
    std::optional<CivilDate> default_value_;
    std::optional<Prop<CalendarMode>> mode_;
    CalendarMode default_mode_{CalendarMode::Month};
    Prop<bool> fullscreen_{true};
    Prop<bool> show_week_{false};
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    Prop<bool> show_today_{false};
    Prop<std::optional<CalendarRange>> range_{std::optional<CalendarRange>{}};
    Prop<DisabledDate> disabled_date_{DisabledDate{}};
    Prop<PickerLocale> locale_{PickerLocale{}};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    CalendarRef ref_;
    std::function<CivilDate()> clock_;
    Prop<CalendarCellRenderer> cell_{CalendarCellRenderer{}};
    Prop<CalendarCellRenderer> full_cell_{CalendarCellRenderer{}};
    Prop<CalendarCellRenderer> date_cell_{CalendarCellRenderer{}};
    Prop<CalendarCellRenderer> month_cell_{CalendarCellRenderer{}};
    Prop<CalendarHeaderRenderer> header_{CalendarHeaderRenderer{}};
    std::function<void(CivilDate)> on_change_;
    std::function<void(CivilDate, CalendarSelectInfo)> on_select_;
    std::function<void(CivilDate, CalendarMode)> on_panel_;
    LayoutStyle layout_;
};

void Calendar(CalendarProps props = {});
} // namespace ryn
