#pragma once

#include <ryn/time_picker.hpp>
#include <unordered_map>

namespace ryn::detail {

using TimeColumn = TimePickerColumn;

class TimePickerModel final {
public:
    TimePickerModel(TimePickerRules, TimePickerPrecision, TimePickerValue, bool controlled, TimeOfDay panel = {});
    void configure(TimePickerRules, TimePickerPrecision);
    void reconcile(TimePickerValue);
    void configure_display(bool twelve, String format, PickerLocale locale);
    [[nodiscard]] String format(TimeOfDay) const;
    [[nodiscard]] int row_count(TimeColumn) const;
    [[nodiscard]] int row_value(TimeColumn) const;
    [[nodiscard]] bool column_visible(TimeColumn) const;
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

    void owner_guard(std::function<bool()> value) {
        owner_guard_ = std::move(value);
    }

private:
    [[nodiscard]] bool owner_active() const {
        return !owner_guard_ || owner_guard_();
    }

    std::function<bool()> owner_guard_;
    static void validate(const TimePickerRules&, TimePickerPrecision);
    [[nodiscard]] TimeOfDay normalized(TimeOfDay) const;
    TimePickerRules rules_;
    TimePickerPrecision precision_;
    TimePickerValue value_;
    TimeOfDay default_panel_;
    TimeOfDay pending_;
    String draft_;
    using CompletionCache = std::unordered_map<unsigned, TimePickerValue>;
    std::shared_ptr<CompletionCache> completions_{std::make_shared<CompletionCache>()};
    bool twelve_{};
    String format_;
    PickerLocale locale_;
    bool controlled_{};
    bool text_dirty_{};
};

} // namespace ryn::detail
