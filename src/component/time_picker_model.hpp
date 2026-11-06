#pragma once

#include <ryn/time_picker.hpp>
#include "component/picker_format.hpp"
#include <unordered_map>

namespace ryn::detail {

using TimeColumn = TimePickerColumn;

class TimePickerModel final {
public:
    TimePickerModel(TimePickerRules, TimePickerPrecision, TimePickerValue, bool controlled, TimeOfDay panel = {});
    void configure(TimePickerRules, TimePickerPrecision);
    void reconcile(TimePickerValue);

    void reconcile_candidate(TimePickerValue value) {
        value_ = value;
    }

    void configure_display(bool twelve, String format, PickerLocale locale, bool preserve_edit = false);
    void freeze_edit();
    void finish_edit();
    [[nodiscard]] String format(TimeOfDay) const;
    [[nodiscard]] int row_count(TimeColumn) const;
    [[nodiscard]] int row_value(TimeColumn) const;
    [[nodiscard]] bool column_visible(TimeColumn) const;
    void begin();
    void cancel();
    void edit(String);
    void stage_edit(String);

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
    [[nodiscard]] bool allowed(TimeOfDay, TimePickerPrecision) const;

    [[nodiscard]] TimePickerPrecision request_precision(bool from_text) const {
        return from_text && edit_format_ ? edit_format_->precision : precision_;
    }

    [[nodiscard]] std::optional<TimePickerValue> text_value() const;
    [[nodiscard]] TimePickerValue candidate(TimeColumn, int) const;
    bool select(TimeColumn, int);
    bool navigate(TimeColumn, int direction, bool boundary = false);

    [[nodiscard]] bool confirmable() const {
        return allowed(pending_);
    }

    [[nodiscard]] std::optional<TimePickerValue> request(TimePickerValue, bool from_text = false);

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

    struct EditFormat final {
        PickerEditFormat input;
        TimePickerPrecision precision{TimePickerPrecision::Second};
        bool twelve{};
    };

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
    std::shared_ptr<const EditFormat> edit_format_;
    bool controlled_{};
    bool text_dirty_{};
};

} // namespace ryn::detail
