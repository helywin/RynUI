#include "component/time_picker_model.hpp"

namespace ryn {

std::optional<TimeOfDay> parse_time_of_day(std::string_view text, TimePickerPrecision precision) {
    if (precision != TimePickerPrecision::Second && precision != TimePickerPrecision::Minute) {
        throw std::invalid_argument("TimePicker precision is invalid");
    }
    const auto size = precision == TimePickerPrecision::Second ? 8U : 5U;
    if (text.size() != size || text[2] != ':' || (size == 8 && text[5] != ':')) {
        return {};
    }
    for (std::size_t i = 0; i < size; ++i) {
        if (i != 2 && i != 5 && (text[i] < '0' || text[i] > '9')) {
            return {};
        }
    }
    const int hour = (text[0] - '0') * 10 + text[1] - '0';
    const int minute = (text[3] - '0') * 10 + text[4] - '0';
    const int second = size == 8 ? (text[6] - '0') * 10 + text[7] - '0' : 0;
    if (hour > 23 || minute > 59 || second > 59) {
        return {};
    }
    return TimeOfDay{hour, minute, second};
}

String format_time_of_day(TimeOfDay time, TimePickerPrecision precision) {
    if (precision != TimePickerPrecision::Second && precision != TimePickerPrecision::Minute) {
        throw std::invalid_argument("TimePicker precision is invalid");
    }
    char text[8]{static_cast<char>('0' + time.hour() / 10),   static_cast<char>('0' + time.hour() % 10),   ':',
                 static_cast<char>('0' + time.minute() / 10), static_cast<char>('0' + time.minute() % 10), ':',
                 static_cast<char>('0' + time.second() / 10), static_cast<char>('0' + time.second() % 10)};
    return String::from_utf8(std::string_view{text, precision == TimePickerPrecision::Second ? 8U : 5U}).value();
}

} // namespace ryn

namespace ryn::detail {

void TimePickerModel::validate(const TimePickerRules& r, TimePickerPrecision precision) {
    if (r.hour_step < 1 || r.hour_step > 24 || r.minute_step < 1 || r.minute_step > 60 || r.second_step < 1 ||
        r.second_step > 60 || (r.min && r.max && *r.min > *r.max)) {
        throw std::invalid_argument("TimePicker requires finite positive steps within 24/60/60 and min <= max");
    }
    if (precision != TimePickerPrecision::Minute && precision != TimePickerPrecision::Second) {
        throw std::invalid_argument("TimePicker precision is invalid");
    }
}

TimePickerModel::TimePickerModel(TimePickerRules rules, TimePickerPrecision precision, TimePickerValue value,
                                 bool controlled, TimeOfDay panel)
    : rules_(std::move(rules)), precision_(precision), value_(value), default_panel_(panel), controlled_(controlled) {
    validate(rules_, precision_);
    begin();
}

void TimePickerModel::configure(TimePickerRules rules, TimePickerPrecision precision) {
    validate(rules, precision);
    rules_ = std::move(rules);
    precision_ = precision;
    pending_ = normalized(pending_);
    if (!text_dirty_) {
        draft_ = value_ ? format_time_of_day(*value_, precision_) : String{};
    }
}

void TimePickerModel::reconcile(TimePickerValue value) {
    value_ = value;
    begin();
}

TimeOfDay TimePickerModel::normalized(TimeOfDay time) const {
    return precision_ == TimePickerPrecision::Minute ? TimeOfDay{time.hour(), time.minute()} : time;
}

void TimePickerModel::begin() {
    pending_ = normalized(value_.value_or(default_panel_));
    text_dirty_ = false;
    draft_ = value_ ? format_time_of_day(*value_, precision_) : String{};
}

void TimePickerModel::cancel() {
    begin();
}

void TimePickerModel::edit(String text) {
    draft_ = std::move(text);
    text_dirty_ = true;
    const auto parsed = text_value();
    if (parsed && *parsed) {
        pending_ = **parsed;
    }
}

bool TimePickerModel::allowed(TimeOfDay time) const {
    return (precision_ != TimePickerPrecision::Minute || time.second() == 0) && time.hour() % rules_.hour_step == 0 &&
           time.minute() % rules_.minute_step == 0 && time.second() % rules_.second_step == 0 &&
           (!rules_.min || time >= *rules_.min) && (!rules_.max || time <= *rules_.max) &&
           (!rules_.disabled || !rules_.disabled(time));
}

std::optional<TimePickerValue> TimePickerModel::text_value() const {
    if (draft_.empty()) {
        return TimePickerValue{};
    }
    const auto parsed = parse_time_of_day(draft_.view().bytes(), precision_);
    if (!parsed || !allowed(*parsed)) {
        return {};
    }
    return parsed;
}

TimePickerValue TimePickerModel::candidate(TimeColumn column, int value) const {
    if (column < TimeColumn::Hour || column > TimeColumn::Second || value < 0 ||
        value >= (column == TimeColumn::Hour ? 24 : 60) ||
        (column == TimeColumn::Second && precision_ == TimePickerPrecision::Minute)) {
        return {};
    }
    const TimeOfDay result{column == TimeColumn::Hour ? value : pending_.hour(),
                           column == TimeColumn::Minute ? value : pending_.minute(),
                           column == TimeColumn::Second ? value : pending_.second()};
    // Only change this column. Other columns stay visible and can be corrected independently.
    // A row is enabled if at least one completion of its prefix is selectable.
    if (column == TimeColumn::Second) {
        return allowed(result) ? TimePickerValue{result} : TimePickerValue{};
    }
    const int minute_start = column == TimeColumn::Minute ? value : 0;
    const int minute_end = column == TimeColumn::Minute ? value + 1 : 60;
    if (allowed(result)) {
        return result;
    }
    for (int minute = minute_start; minute < minute_end; ++minute) {
        const int seconds = precision_ == TimePickerPrecision::Minute ? 1 : 60;
        for (int second = 0; second < seconds; ++second) {
            const TimeOfDay trial{result.hour(), minute, second};
            if (allowed(trial)) {
                return trial;
            }
        }
    }
    return {};
}

bool TimePickerModel::select(TimeColumn column, int value) {
    const auto next = candidate(column, value);
    if (!next) {
        return false;
    }
    const bool changed = pending_ != *next || text_dirty_;
    pending_ = *next;
    text_dirty_ = false;
    draft_ = format_time_of_day(pending_, precision_);
    return changed;
}

bool TimePickerModel::navigate(TimeColumn column, int direction, bool boundary) {
    const int count = column == TimeColumn::Hour ? 24 : 60;
    const int current = column == TimeColumn::Hour     ? pending_.hour()
                        : column == TimeColumn::Minute ? pending_.minute()
                                                       : pending_.second();
    const int delta = direction < 0 ? -1 : 1;
    int index = boundary ? (delta > 0 ? 0 : count - 1) : current + delta;
    for (; index >= 0 && index < count; index += delta) {
        if (candidate(column, index)) {
            return select(column, index);
        }
    }
    return false;
}

std::optional<TimePickerValue> TimePickerModel::request(TimePickerValue value) {
    if (value) {
        value = normalized(*value);
        if (!allowed(*value)) {
            return {};
        }
    }
    if (!controlled_) {
        value_ = value;
    }
    return value;
}

} // namespace ryn::detail
