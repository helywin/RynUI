#pragma once

#include <ryn/time_picker.hpp>

namespace ryn::detail {

enum class TimeColumn { Hour, Minute, Second };

class TimePickerModel final {
public:
    TimePickerModel(TimePickerRules, TimePickerPrecision, TimePickerValue, bool controlled, TimeOfDay panel = {});
    void configure(TimePickerRules, TimePickerPrecision);
    void reconcile(TimePickerValue);
    void begin();
    void cancel();
    void edit(String);

    [[nodiscard]] TimePickerValue value() const {
        return value_;
    }

    [[nodiscard]] TimeOfDay panel() const {
        return pending_;
    }

    [[nodiscard]] const String& draft() const {
        return draft_;
    }

    [[nodiscard]] bool text_dirty() const {
        return text_dirty_;
    }

    [[nodiscard]] TimePickerPrecision precision() const {
        return precision_;
    }

    [[nodiscard]] const TimePickerRules& rules() const {
        return rules_;
    }

    [[nodiscard]] bool allowed(TimeOfDay) const;
    [[nodiscard]] std::optional<TimePickerValue> text_value() const;
    [[nodiscard]] TimePickerValue candidate(TimeColumn, int) const;
    bool select(TimeColumn, int);
    bool navigate(TimeColumn, int direction, bool boundary = false);

    [[nodiscard]] bool confirmable() const {
        return allowed(pending_);
    }

    [[nodiscard]] std::optional<TimePickerValue> request(TimePickerValue);

private:
    static void validate(const TimePickerRules&, TimePickerPrecision);
    [[nodiscard]] TimeOfDay normalized(TimeOfDay) const;
    TimePickerRules rules_;
    TimePickerPrecision precision_;
    TimePickerValue value_;
    TimeOfDay default_panel_;
    TimeOfDay pending_;
    String draft_;
    bool controlled_{};
    bool text_dirty_{};
};

} // namespace ryn::detail
