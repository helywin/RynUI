#pragma once
#include <ryn/date_picker.hpp>
#include <array>

namespace ryn::detail {
struct DatePickerRules final {
    DatePickerValue min;
    DatePickerValue max;
    DisabledDate disabled;
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

private:
    DatePickerValue value_;
    bool controlled_{};
    CivilDate active_;
    DatePickerRules rules_;
};
} // namespace ryn::detail
