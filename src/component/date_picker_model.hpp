#pragma once
#include <ryn/date_picker.hpp>
#include <array>

namespace ryn::detail {
struct DatePickerRules final {
    DatePickerValue min;
    DatePickerValue max;
    DisabledDate disabled;
    std::function<bool(CivilDate, const DatePickerDisabledInfo&)> info;
    DatePickerValue from;
    DatePickerMode type{DatePickerMode::Date};
};

class DatePickerModel final {
public:
    DatePickerModel(DatePickerValue value, bool controlled, CivilDate panel, DatePickerRules rules = {});
    void configure(DatePickerRules rules);
    void reconcile(DatePickerValue value);
    [[nodiscard]] std::optional<DatePickerValue> request(DatePickerValue value);
    [[nodiscard]] bool eligible(CivilDate value) const;
    bool navigate(int months);
    bool move(int days);
    bool boundary(bool last);
    [[nodiscard]] std::array<DatePickerValue, 42> grid() const;
    void view(DatePickerMode, PickerLocale);

    void panel_date(CivilDate value) {
        active_ = value;
    }

    [[nodiscard]] DatePickerMode mode() const {
        return mode_;
    }

    [[nodiscard]] DatePickerValue cell(std::size_t) const;
    [[nodiscard]] std::size_t cell_count() const;
    [[nodiscard]] CivilDate normalized(CivilDate) const;

    [[nodiscard]] DatePickerValue value() const {
        return value_;
    }

    [[nodiscard]] CivilDate active() const {
        return active_;
    }

    [[nodiscard]] CivilDate panel() const {
        return CivilDate{active_.year(), active_.month(), 1};
    }

    [[nodiscard]] static CivilDate utc_date();

    void owner_guard(std::function<bool()> value) {
        owner_guard_ = std::move(value);
    }

private:
    [[nodiscard]] bool owner_active() const {
        return !owner_guard_ || owner_guard_();
    }

    std::function<bool()> owner_guard_;
    DatePickerValue value_;
    bool controlled_{};
    CivilDate active_;
    DatePickerRules rules_;
    DatePickerMode mode_{DatePickerMode::Date};
    PickerLocale locale_;
};
} // namespace ryn::detail
