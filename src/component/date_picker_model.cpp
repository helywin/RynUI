#include "component/date_picker_model.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace ryn {
bool CivilDate::valid(int year, unsigned month, unsigned day) noexcept {
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}}.ok();
}

CivilDate::CivilDate(int year, unsigned month, unsigned day) : year_(year), month_(month), day_(day) {
    if (!valid(year, month, day)) {
        throw std::invalid_argument("CivilDate requires a valid Gregorian date in years 1..9999");
    }
}

DatePickerValue parse_iso_date(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return {};
    }
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (text[i] < '0' || text[i] > '9') {
            return {};
        }
        if (i < 4) {
            year = year * 10 + text[i] - '0';
        } else if (i < 7) {
            month = month * 10 + text[i] - '0';
        } else {
            day = day * 10 + text[i] - '0';
        }
    }
    if (!CivilDate::valid(year, month, day)) {
        return {};
    }
    return CivilDate{year, month, day};
}

String format_iso_date(CivilDate value) {
    std::string result = "0000-00-00";
    int year = value.year();
    for (int i = 3; i >= 0; --i) {
        result[static_cast<std::size_t>(i)] = static_cast<char>('0' + year % 10);
        year /= 10;
    }
    result[5] = static_cast<char>('0' + value.month() / 10);
    result[6] = static_cast<char>('0' + value.month() % 10);
    result[8] = static_cast<char>('0' + value.day() / 10);
    result[9] = static_cast<char>('0' + value.day() % 10);
    return String::from_utf8(result).value();
}
} // namespace ryn

namespace ryn::detail {
namespace {
std::chrono::sys_days days(CivilDate date) {
    return std::chrono::year{date.year()} / date.month() / date.day();
}

CivilDate civil(std::chrono::sys_days date) {
    const std::chrono::year_month_day value{date};
    return {int(value.year()), unsigned(value.month()), unsigned(value.day())};
}

CivilDate clamped(CivilDate date, const DatePickerRules& rules) {
    return std::clamp(date, rules.min.value_or(CivilDate{1, 1, 1}), rules.max.value_or(CivilDate{9999, 12, 31}));
}
} // namespace

DatePickerModel::DatePickerModel(DatePickerValue value, bool controlled, CivilDate panel, DatePickerRules rules)
    : value_(value), controlled_(controlled), active_(value.value_or(panel)) {
    configure(std::move(rules));
}

void DatePickerModel::configure(DatePickerRules rules) {
    if (rules.min && rules.max && *rules.min > *rules.max) {
        throw std::invalid_argument("DatePicker minDate exceeds maxDate");
    }
    rules_ = std::move(rules);
    active_ = clamped(active_, rules_);
}

void DatePickerModel::reconcile(DatePickerValue value) {
    if (value_ != value) {
        value_ = value;
        if (value) {
            active_ = clamped(*value, rules_);
        }
    }
}

bool DatePickerModel::eligible(CivilDate date) const {
    return (!rules_.min || date >= *rules_.min) && (!rules_.max || date <= *rules_.max) &&
           (!rules_.disabled || !rules_.disabled(date));
}

std::optional<DatePickerValue> DatePickerModel::request(DatePickerValue value) {
    if (value && !eligible(*value)) {
        return {};
    }
    if (!controlled_) {
        value_ = value;
    }
    return std::optional<DatePickerValue>{value};
}

bool DatePickerModel::navigate(int months) {
    const int index = std::clamp((active_.year() - 1) * 12 + int(active_.month()) - 1 + months, 0, 9999 * 12 - 1);
    const int year = index / 12 + 1;
    const unsigned month = unsigned(index % 12 + 1);
    unsigned day = active_.day();
    while (!CivilDate::valid(year, month, day)) {
        --day;
    }
    const auto next = clamped(CivilDate{year, month, day}, rules_);
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

bool DatePickerModel::move(int count) {
    const auto next = clamped(civil(std::clamp(days(active_) + std::chrono::days{count}, days(CivilDate{1, 1, 1}),
                                               days(CivilDate{9999, 12, 31}))),
                              rules_);
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

bool DatePickerModel::boundary(bool last) {
    unsigned day = last ? 31 : 1;
    while (!CivilDate::valid(active_.year(), active_.month(), day)) {
        --day;
    }
    const auto next = clamped(CivilDate{active_.year(), active_.month(), day}, rules_);
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

std::array<DatePickerValue, 42> DatePickerModel::grid() const {
    std::array<DatePickerValue, 42> result;
    const auto first = days(panel());
    const auto start = first - std::chrono::days{std::chrono::weekday{first}.c_encoding()};
    for (int i = 0; i < 42; ++i) {
        const auto day = start + std::chrono::days{i};
        if (day >= days(CivilDate{1, 1, 1}) && day <= days(CivilDate{9999, 12, 31})) {
            result[std::size_t(i)] = civil(day);
        }
    }
    return result;
}

CivilDate DatePickerModel::utc_date() {
    return civil(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
}
} // namespace ryn::detail
