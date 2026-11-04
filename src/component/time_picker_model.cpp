#include "component/time_picker_model.hpp"
#include <algorithm>

namespace ryn {

std::optional<TimeOfDay> parse_time_of_day(std::string_view text, TimePickerPrecision precision) {
    const auto format = precision == TimePickerPrecision::Minute        ? "HH:mm"
                        : precision == TimePickerPrecision::Second      ? "HH:mm:ss"
                        : precision == TimePickerPrecision::Millisecond ? "HH:mm:ss.SSS"
                                                                        : "";
    if (std::string_view{format}.empty()) {
        throw std::invalid_argument("Invalid time precision");
    }
    return parse_time_of_day(text, format);
}

String format_time_of_day(TimeOfDay value, TimePickerPrecision precision) {
    const auto format = precision == TimePickerPrecision::Minute        ? "HH:mm"
                        : precision == TimePickerPrecision::Second      ? "HH:mm:ss"
                        : precision == TimePickerPrecision::Millisecond ? "HH:mm:ss.SSS"
                                                                        : "";
    if (std::string_view{format}.empty()) {
        throw std::invalid_argument("Invalid time precision");
    }
    return format_time_of_day(value, format);
}

} // namespace ryn

namespace ryn::detail {

void TimePickerModel::validate(const TimePickerRules& r, TimePickerPrecision precision) {
    if (r.hour_step < 1 || r.hour_step > 24 || r.minute_step < 1 || r.minute_step > 60 || r.second_step < 1 ||
        r.second_step > 60 || r.millisecond_step < 1 || r.millisecond_step > 1000 ||
        (r.min && r.max && *r.min > *r.max)) {
        throw std::invalid_argument("TimePicker requires finite positive steps within 24/60/60 and min <= max");
    }
    if (precision != TimePickerPrecision::Minute && precision != TimePickerPrecision::Second &&
        precision != TimePickerPrecision::Millisecond) {
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
    completions_ = std::make_shared<CompletionCache>();
    precision_ = precision;
    pending_ = normalized(pending_);
    if (!text_dirty_) {
        draft_ = value_ ? format(*value_) : String{};
    }
}

void TimePickerModel::reconcile(TimePickerValue value) {
    value_ = value;
    begin();
}

TimeOfDay TimePickerModel::normalized(TimeOfDay time) const {
    return precision_ == TimePickerPrecision::Minute   ? TimeOfDay{time.hour(), time.minute()}
           : precision_ == TimePickerPrecision::Second ? TimeOfDay{time.hour(), time.minute(), time.second()}
                                                       : time;
}

void TimePickerModel::begin() {
    pending_ = normalized(value_.value_or(default_panel_));
    text_dirty_ = false;
    draft_ = value_ ? format(*value_) : String{};
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
    return owner_active() && (precision_ != TimePickerPrecision::Minute || time.second() == 0) &&
           time.hour() % rules_.hour_step == 0 && time.minute() % rules_.minute_step == 0 &&
           time.second() % rules_.second_step == 0 && time.millisecond() % rules_.millisecond_step == 0 &&
           (precision_ == TimePickerPrecision::Millisecond || time.millisecond() == 0) &&
           (!rules_.disabled_hours || !rules_.disabled_hours(time.hour())) && owner_active() &&
           (!rules_.disabled_minutes || !rules_.disabled_minutes(time.hour(), time.minute())) && owner_active() &&
           (!rules_.disabled_seconds || !rules_.disabled_seconds(time.hour(), time.minute(), time.second())) &&
           owner_active() &&
           (!rules_.disabled_milliseconds ||
            !rules_.disabled_milliseconds(time.hour(), time.minute(), time.second(), time.millisecond())) &&
           owner_active() && (!rules_.min || time >= *rules_.min) && (!rules_.max || time <= *rules_.max) &&
           (!rules_.disabled || !rules_.disabled(time)) && owner_active();
}

std::optional<TimePickerValue> TimePickerModel::text_value() const {
    if (draft_.empty()) {
        return TimePickerValue{};
    }
    const auto parsed = parse_time_of_day(
        draft_.view().bytes(),
        format_.empty() ? (twelve_ ? (precision_ == TimePickerPrecision::Minute        ? "h:mm a"
                                      : precision_ == TimePickerPrecision::Millisecond ? "h:mm:ss.SSS a"
                                                                                       : "h:mm:ss a")
                                   : (precision_ == TimePickerPrecision::Minute        ? "HH:mm"
                                      : precision_ == TimePickerPrecision::Millisecond ? "HH:mm:ss.SSS"
                                                                                       : "HH:mm:ss"))
                        : format_.bytes(),
        locale_);
    if (!parsed || !allowed(*parsed)) {
        return {};
    }
    return parsed;
}

void TimePickerModel::configure_display(bool twelve, String pattern, PickerLocale locale) {
    locale.validate();
    if (!pattern.empty()) {
        validate_time_format(pattern.bytes());
    }
    twelve_ = twelve;
    format_ = std::move(pattern);
    locale_ = std::move(locale);
    if (!text_dirty_) {
        draft_ = value_ ? format(*value_) : String{};
    }
}

String TimePickerModel::format(TimeOfDay time) const {
    const auto pattern = format_.empty()
                             ? (twelve_ ? (precision_ == TimePickerPrecision::Minute        ? "h:mm a"
                                           : precision_ == TimePickerPrecision::Millisecond ? "h:mm:ss.SSS a"
                                                                                            : "h:mm:ss a")
                                        : (precision_ == TimePickerPrecision::Minute        ? "HH:mm"
                                           : precision_ == TimePickerPrecision::Millisecond ? "HH:mm:ss.SSS"
                                                                                            : "HH:mm:ss"))
                             : format_.bytes();
    return format_time_of_day(time, pattern, locale_);
}

int TimePickerModel::row_count(TimeColumn column) const {
    return column == TimeColumn::Hour          ? (twelve_ ? 12 : 24)
           : column == TimeColumn::Millisecond ? 1000
           : column == TimeColumn::Meridiem    ? 2
                                               : 60;
}

int TimePickerModel::row_value(TimeColumn column) const {
    return column == TimeColumn::Hour          ? (twelve_ ? pending_.hour() % 12 : pending_.hour())
           : column == TimeColumn::Minute      ? pending_.minute()
           : column == TimeColumn::Second      ? pending_.second()
           : column == TimeColumn::Millisecond ? pending_.millisecond()
                                               : pending_.hour() / 12;
}

bool TimePickerModel::column_visible(TimeColumn column) const {
    return column == TimeColumn::Hour || column == TimeColumn::Minute ||
           (column == TimeColumn::Second && precision_ != TimePickerPrecision::Minute) ||
           (column == TimeColumn::Millisecond && precision_ == TimePickerPrecision::Millisecond) ||
           (column == TimeColumn::Meridiem && twelve_);
}

TimePickerValue TimePickerModel::candidate(TimeColumn column, int value) const {
    if (!owner_active() || column < TimeColumn::Hour || column > TimeColumn::Meridiem || value < 0 ||
        value >= row_count(column) || !column_visible(column)) {
        return {};
    }
    if (column == TimeColumn::Meridiem) {
        auto trial = *this;
        trial.pending_ =
            TimeOfDay{pending_.hour() % 12 + value * 12, pending_.minute(), pending_.second(), pending_.millisecond()};
        if (const auto same = trial.candidate(TimeColumn::Hour, pending_.hour() % 12)) {
            return same;
        }
        for (int row = 0; row < 12; ++row) {
            if (const auto candidate = trial.candidate(TimeColumn::Hour, row)) {
                return candidate;
            }
        }
        return {};
    }
    const int hour = column == TimeColumn::Hour       ? value + (twelve_ ? pending_.hour() / 12 * 12 : 0)
                     : column == TimeColumn::Meridiem ? pending_.hour() % 12 + value * 12
                                                      : pending_.hour();
    const int minute = column == TimeColumn::Minute ? value : pending_.minute();
    const int second = column == TimeColumn::Second ? value : pending_.second();
    const int ms = column == TimeColumn::Millisecond ? value : pending_.millisecond();
    const TimeOfDay preferred{hour, minute, second, ms};
    if (allowed(preferred)) {
        return preferred;
    }
    if (!owner_active() || column == TimeColumn::Millisecond || (rules_.min && hour < rules_.min->hour()) ||
        (rules_.max && hour > rules_.max->hour()) || hour % rules_.hour_step) {
        return {};
    }
    const bool hour_disabled = rules_.disabled_hours && rules_.disabled_hours(hour);
    if (!owner_active() || hour_disabled) {
        return {};
    }
    const unsigned key = column == TimeColumn::Hour ? static_cast<unsigned>(hour)
                         : column == TimeColumn::Minute
                             ? static_cast<unsigned>(24 + hour * 60 + minute)
                             : static_cast<unsigned>(24 + 24 * 60 + (hour * 60 + minute) * 60 + second);
    if (const auto cached = completions_->find(key); cached != completions_->end()) {
        return cached->second;
    }
    int mstart = column == TimeColumn::Hour ? 0 : minute;
    int mend = column == TimeColumn::Hour ? 60 : minute + 1;
    if (rules_.min && hour == rules_.min->hour()) {
        mstart = std::max(mstart, rules_.min->minute());
    }
    if (rules_.max && hour == rules_.max->hour()) {
        mend = std::min(mend, rules_.max->minute() + 1);
    }
    for (int m = mstart; m < mend; ++m) {
        if (m % rules_.minute_step) {
            continue;
        }
        const bool disabled = rules_.disabled_minutes && rules_.disabled_minutes(hour, m);
        if (!owner_active()) {
            return {};
        }
        if (disabled) {
            continue;
        }
        int sstart = column == TimeColumn::Second ? second : 0;
        int send = column == TimeColumn::Second ? second + 1 : precision_ == TimePickerPrecision::Minute ? 1 : 60;
        if (rules_.min && hour == rules_.min->hour() && m == rules_.min->minute()) {
            sstart = std::max(sstart, rules_.min->second());
        }
        if (rules_.max && hour == rules_.max->hour() && m == rules_.max->minute()) {
            send = std::min(send, rules_.max->second() + 1);
        }
        for (int sec = sstart; sec < send; ++sec) {
            if (sec % rules_.second_step) {
                continue;
            }
            const bool disabled_second = rules_.disabled_seconds && rules_.disabled_seconds(hour, m, sec);
            if (!owner_active()) {
                return {};
            }
            if (disabled_second) {
                continue;
            }
            int msstart{};
            int msend = precision_ == TimePickerPrecision::Millisecond ? 1000 : 1;
            if (rules_.min && hour == rules_.min->hour() && m == rules_.min->minute() && sec == rules_.min->second()) {
                msstart = rules_.min->millisecond();
            }
            if (rules_.max && hour == rules_.max->hour() && m == rules_.max->minute() && sec == rules_.max->second()) {
                msend = std::min(msend, rules_.max->millisecond() + 1);
            }
            msstart = (msstart + rules_.millisecond_step - 1) / rules_.millisecond_step * rules_.millisecond_step;
            for (int milli = msstart; milli < msend; milli += rules_.millisecond_step) {
                const TimeOfDay trial{hour, m, sec, milli};
                if (allowed(trial)) {
                    completions_->emplace(key, trial);
                    return trial;
                }
                if (!owner_active()) {
                    return {};
                }
            }
        }
    }
    completions_->emplace(key, TimePickerValue{});
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
    draft_ = format(pending_);
    return changed;
}

bool TimePickerModel::navigate(TimeColumn column, int direction, bool boundary) {
    const int count = row_count(column);
    const int current = row_value(column);
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
